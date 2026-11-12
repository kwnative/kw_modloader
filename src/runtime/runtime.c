/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "runtime.h"
#include "../core/core.h"
#include <kwf/patch_query.h>
struct loaded_plugin {
    HMODULE module;
    const struct kwf_plugin *api;
    struct kwf_context context;
    uint32_t state, index;
};
static struct kwf_manifest plugin_manifests[KWF_MAX_PLUGINS];
static struct loaded_plugin loaded_plugins[KWF_MAX_PLUGINS];
static struct kwf_registry patch_registry;
static uint32_t count, startup_phase, startup_status = KWF_BAD_PHASE;
static wchar_t runtime_root_directory[MAX_PATH], plugin_directory[MAX_PATH];
static HANDLE runtime_log_file = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION runtime_log_lock, patch_registry_lock;
static DWORD startup_thread_id;
static INIT_ONCE runtime_init_once = INIT_ONCE_STATIC_INIT;
static HMODULE loader_module;
static uint32_t process_role;
static struct {
    const char *id;
    uint32_t major, owner;
    const void *api;
} published_services[32];
static uint32_t service_count;
static void KWF_CALL runtime_log_message(void *owner, const char *message)
{
    char line[768];
    DWORD written;
    const struct loaded_plugin *p = owner;
    const char *id = p ? plugin_manifests[p->index].id : "framework";
    int n = snprintf(line, sizeof(line), "[%lu ms tid=%lu] %s: %s\r\n", (unsigned long)GetTickCount(), (unsigned long)GetCurrentThreadId(), id,
                     message ? message : "(null)");
    if (n < 0)
        return;
    line[sizeof(line) - 1] = 0;
    EnterCriticalSection(&runtime_log_lock);
    OutputDebugStringA(line);
    if (runtime_log_file != INVALID_HANDLE_VALUE)
        WriteFile(runtime_log_file, line, (DWORD)strlen(line), &written, NULL);
    LeaveCriticalSection(&runtime_log_lock);
}
static void log_code(void *owner, const char *operation, uint32_t code)
{
    char message[192];
    snprintf(message, sizeof(message), "%s (code=%lu)", operation, (unsigned long)code);
    runtime_log_message(owner, message);
}
static uint32_t plugin_owner_id(void *owner)
{
    uint32_t i;
    for (i = 0; i < count; ++i)
        if (owner == &loaded_plugins[i])
            return i + 1;
    return 0;
}
static int is_readable_span(uintptr_t address, uint32_t length)
{
    MEMORY_BASIC_INFORMATION info;
    uintptr_t end = address + length;
    while (address < end) {
        uintptr_t next;
        if (!VirtualQuery((void *)address, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return 0;
        next = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next <= address)
            return 0;
        address = next;
    }
    return 1;
}
static uint32_t KWF_CALL reserve_plugin_patch(void *owner, uintptr_t address, uint32_t length)
{
    uint32_t id = plugin_owner_id(owner), result;
    if (startup_phase != 1 || GetCurrentThreadId() != startup_thread_id)
        return KWF_BAD_PHASE;
    if (!id || !length || address > UINTPTR_MAX - length || !is_readable_span(address, length))
        return KWF_ERROR;
    result = kwf_reserve(&patch_registry, id, address, length);
    if (result == KWF_OK) {
        struct kwf_patch *p = kwf_patch_find(&patch_registry, id, address, length);
        memcpy(p->original, (void *)address, length);
    }
    return result;
}
static uint32_t protected_write(uintptr_t address, const void *expected, const void *bytes, uint32_t length)
{
    DWORD previous, ignored;
    uint32_t result = KWF_OK;
    if (!VirtualProtect((void *)address, length, PAGE_EXECUTE_READWRITE, &previous))
        return KWF_ERROR;
    if (length == 4 && !(address & 3U)) {
        /* Preserve atomic publication for Win32 vtable slots and scalar
         * operands. A foreign change after validation is not ours to replace. */
        LONG wanted, replacement;
        memcpy(&wanted, expected, 4);
        memcpy(&replacement, bytes, 4);
        if (InterlockedCompareExchange((volatile LONG *)address, replacement, wanted) != wanted)
            result = KWF_CONFLICT;
    } else if (memcmp((void *)address, expected, length))
        result = KWF_CONFLICT;
    else
        memcpy((void *)address, bytes, length);
    if (result == KWF_OK)
        FlushInstructionCache(GetCurrentProcess(), (void *)address, length);
    /* The write has happened even if restoring protection fails; retain the
     * ownership record so rollback can still recognize these bytes. */
    if (!VirtualProtect((void *)address, length, previous, &ignored))
        runtime_log_message(NULL, "could not restore page protection after owned write");
    return result;
}
static uint32_t KWF_CALL write_plugin_patch(void *owner, uintptr_t address, const void *expected, const void *replacement, uint32_t length)
{
    struct kwf_patch *p;
    uint32_t result = KWF_CONFLICT;
    if (startup_phase < 2 || GetCurrentThreadId() != startup_thread_id)
        return KWF_BAD_PHASE;
    if (!expected || !replacement)
        return KWF_ERROR;
    EnterCriticalSection(&patch_registry_lock);
    p = kwf_patch_find(&patch_registry, plugin_owner_id(owner), address, length);
    if (p && !p->written && is_readable_span(address, length) && !memcmp(p->original, expected, length) &&
        !memcmp((void *)address, expected, length)) {
        /* Callers may pass the destination as expected (the FPS adapter does).
         * Use our immutable snapshot after validation, not an aliased live view. */
        result = protected_write(address, p->original, replacement, length);
        if (result == KWF_OK) {
            memcpy(p->replacement, replacement, length);
            p->written = 1;
            result = KWF_OK;
        }
    }
    LeaveCriticalSection(&patch_registry_lock);
    return result;
}
static uint32_t KWF_CALL restore_plugin_patch(void *owner, uintptr_t address, uint32_t length)
{
    struct kwf_patch *p;
    uint32_t result = KWF_CONFLICT;
    if (startup_phase < 2 || GetCurrentThreadId() != startup_thread_id)
        return KWF_BAD_PHASE;
    EnterCriticalSection(&patch_registry_lock);
    p = kwf_patch_find(&patch_registry, plugin_owner_id(owner), address, length);
    if (p && p->written && is_readable_span(address, length) && !memcmp((void *)address, p->replacement, length)) {
        result = protected_write(address, p->replacement, p->original, length);
        if (result == KWF_OK)
            p->written = 0;
    }
    LeaveCriticalSection(&patch_registry_lock);
    return result;
}
static uint32_t KWF_CALL query_patch(void *requester, uintptr_t address, uint32_t length, struct kwf_patch_info *info)
{
    const struct kwf_patch *found = NULL;
    if (startup_phase < 1 || GetCurrentThreadId() != startup_thread_id)
        return KWF_BAD_PHASE;
    if (!plugin_owner_id(requester) || !info || info->size < sizeof(*info) || !length || address > UINTPTR_MAX - length)
        return KWF_ERROR;
    for (uint32_t i = 0; i < patch_registry.count; ++i) {
        const struct kwf_patch *patch = &patch_registry.patches[i];
        if (address < patch->address + patch->length && patch->address < address + length) {
            if (found)
                return KWF_CONFLICT;
            found = patch;
        }
    }
    if (!found)
        return KWF_UNSUPPORTED;
    *info = (struct kwf_patch_info){
        sizeof(*info), found->length, found->written, loaded_plugins[found->owner - 1].state, found->address, plugin_manifests[found->owner - 1].id};
    return KWF_OK;
}
static uint32_t KWF_CALL query_original(void *requester, uintptr_t address, uint32_t length, void *bytes)
{
    struct kwf_patch_info info = {0};
    info.size = sizeof(info);
    uint32_t result = query_patch(requester, address, length, &info);
    if (result != KWF_OK)
        return result;
    if (!bytes || address < info.address || length > info.length || address - info.address > info.length - length)
        return KWF_CONFLICT;
    for (uint32_t i = 0; i < patch_registry.count; ++i) {
        const struct kwf_patch *patch = &patch_registry.patches[i];
        if (patch->address == info.address && patch->length == info.length) {
            memcpy(bytes, patch->original + (address - info.address), length);
            return KWF_OK;
        }
    }
    return KWF_UNSUPPORTED;
}
static const struct kwf_patch_query patch_query = {sizeof(patch_query), 1, query_patch, query_original};
static const void *KWF_CALL find_plugin_service(void *owner, const char *id, uint32_t major)
{
    uint32_t i;
    if (!plugin_owner_id(owner) || !id)
        return NULL;
    if (major == 1 && !strcmp(id, KWF_PATCH_QUERY_SERVICE))
        return &patch_query;
    for (i = 0; i < service_count; ++i)
        if (published_services[i].major == major && !strcmp(published_services[i].id, id) &&
            (loaded_plugins[published_services[i].owner - 1].state || published_services[i].owner == plugin_owner_id(owner)))
            return published_services[i].api;
    return NULL;
}
static uint32_t KWF_CALL publish_plugin_service(void *owner, const char *id, uint32_t major, const void *api)
{
    uint32_t i, index = plugin_owner_id(owner);
    if (startup_phase != 1 || GetCurrentThreadId() != startup_thread_id)
        return KWF_BAD_PHASE;
    if (!index || !id || !*id || !major || !api || service_count == 32)
        return KWF_ERROR;
    if (!strncmp(id, "kwf.", 4))
        return KWF_CONFLICT;
    for (i = 0; i < service_count; ++i)
        if (published_services[i].major == major && !strcmp(published_services[i].id, id))
            return KWF_CONFLICT;
    published_services[service_count].id = id;
    published_services[service_count].major = major;
    published_services[service_count].owner = index;
    published_services[service_count++].api = api;
    return KWF_OK;
}
static const struct kwf_host host = {sizeof(struct kwf_host), KWF_ABI_MAJOR,      KWF_ABI_MINOR,        KWF_CAP_PATCHES,     runtime_log_message,
                                     reserve_plugin_patch,    write_plugin_patch, restore_plugin_patch, find_plugin_service, publish_plugin_service};
static void discard_preparation(uint32_t owner)
{
    uint32_t i = 0;
    while (i < patch_registry.count) {
        if (patch_registry.patches[i].owner == owner)
            patch_registry.patches[i] = patch_registry.patches[--patch_registry.count];
        else
            ++i;
    }
    i = 0;
    while (i < service_count) {
        if (published_services[i].owner == owner)
            published_services[i] = published_services[--service_count];
        else
            ++i;
    }
}
static int join_directory_path(wchar_t *out, const wchar_t *base, const wchar_t *name)
{
    size_t a = wcslen(base), b = wcslen(name);
    if (a + b + 2 > MAX_PATH)
        return 0;
    memcpy(out, base, a * sizeof(wchar_t));
    out[a] = L'\\';
    memcpy(out + a + 1, name, (b + 1) * sizeof(wchar_t));
    return 1;
}
static int read_manifest(const wchar_t *name, struct kwf_manifest *m)
{
    wchar_t path[MAX_PATH];
    char data[KWF_MANIFEST_LIMIT];
    HANDLE file;
    DWORD size, got;
    int ok;
    if (!join_directory_path(path, plugin_directory, name))
        return 0;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    size = GetFileSize(file, NULL);
    ok = size && size <= sizeof(data) && ReadFile(file, data, size, &got, NULL) && got == size && kwf_manifest_parse(data, size, m);
    CloseHandle(file);
    return ok;
}
static int dependencies_ready(uint32_t index, uint32_t required_state)
{
    uint32_t d, j;
    for (d = 0; d < plugin_manifests[index].dependency_count; ++d) {
        for (j = 0; j < count; ++j)
            if (!strcmp(plugin_manifests[index].required_plugins[d], plugin_manifests[j].id))
                break;
        if (j == count || loaded_plugins[j].state != required_state)
            return 0;
    }
    return 1;
}
static BOOL CALLBACK initialize_plugin_runtime(PINIT_ONCE init, PVOID param, PVOID *context)
{
    WIN32_FIND_DATAW found;
    HANDLE search;
    wchar_t path[MAX_PATH];
    DWORD n;
    uint32_t order[KWF_MAX_PLUGINS], k, planned_count;
    HMODULE image = GetModuleHandleW(NULL);
    uint32_t image_size = 0;
    (void)init;
    (void)param;
    (void)context;
    InitializeCriticalSection(&runtime_log_lock);
    InitializeCriticalSection(&patch_registry_lock);
    startup_thread_id = GetCurrentThreadId();
    n = GetModuleFileNameW(loader_module, runtime_root_directory, MAX_PATH);
    if (!n || n >= MAX_PATH || !wcsrchr(runtime_root_directory, L'\\')) {
        startup_status = KWF_ERROR;
        return TRUE;
    }
    *wcsrchr(runtime_root_directory, L'\\') = 0;
    wchar_t log_name[64] = L"kw_framework.log";
    if (process_role == KWF_ROLE_HEADLESS)
        swprintf(log_name, sizeof(log_name) / sizeof(*log_name), L"kw_framework-bot-%lu.log", (unsigned long)GetCurrentProcessId());
    if (!join_directory_path(plugin_directory, runtime_root_directory, L"plugins") || !join_directory_path(path, runtime_root_directory, log_name)) {
        startup_status = KWF_ERROR;
        return TRUE;
    }
    runtime_log_file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    startup_status = KWF_OK;
    runtime_log_message(NULL, "ABI 1.0 startup outside DllMain; discovery before mutation");
    runtime_log_message(NULL, process_role == KWF_ROLE_RENDERED ? "process role=rendered" : "process role=headless");
    if (image && ((IMAGE_DOS_HEADER *)image)->e_magic == IMAGE_DOS_SIGNATURE) {
        IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)image;
        if (dos->e_lfanew > 0 && dos->e_lfanew < 0x100000) {
            IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)((unsigned char *)image + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE && nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
                image_size = nt->OptionalHeader.SizeOfImage;
        }
    }
    join_directory_path(path, plugin_directory, L"*.kwplugin");
    search = FindFirstFileW(path, &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            struct kwf_manifest m;
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            if (!read_manifest(found.cFileName, &m)) {
                runtime_log_message(NULL, "invalid manifest; skipping this entry before loading any DLL");
                startup_status = KWF_ERROR;
                continue;
            }
            if (!m.enabled || !(m.roles & process_role))
                continue;
            if (count == KWF_MAX_PLUGINS) {
                runtime_log_message(NULL, "too many enabled plugins");
                FindClose(search);
                startup_status = KWF_ERROR;
                return TRUE;
            }
            plugin_manifests[count++] = m;
        } while (FindNextFileW(search, &found));
        FindClose(search);
    } else if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) {
        runtime_log_message(NULL, "manifest enumeration failed");
        startup_status = KWF_ERROR;
        return TRUE;
    }
    planned_count = kwf_plan_available(plugin_manifests, count, order);
    if (planned_count != count) {
        runtime_log_message(NULL, "duplicate plugin/library, dependency missing, or dependency cycle; affected plugins skipped before DLL loading");
        startup_status = KWF_ERROR;
    }
    startup_phase = 1;
    for (k = 0; k < planned_count; ++k) {
        uint32_t i = order[k];
        struct loaded_plugin *p = &loaded_plugins[i];
        wchar_t library[96];
        kwf_query_fn query;
        uint32_t prepare_result;
        p->index = i;
        if (!dependencies_ready(i, 1)) {
            runtime_log_message(p, "dependency prepare failed; skipped");
            continue;
        }
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, plugin_manifests[i].library, -1, library, 96) ||
            !join_directory_path(path, plugin_directory, library))
            continue;
        p->module = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!p->module) {
            log_code(p, "DLL load failed, Win32 error", GetLastError());
            continue;
        }
        query = (kwf_query_fn)(uintptr_t)GetProcAddress(p->module, "KwPluginQuery");
        p->api = query ? query() : NULL;
        if (!kwf_descriptor_valid(&plugin_manifests[i], p->api, process_role)) {
            runtime_log_message(p, "incompatible/missing descriptor");
            continue;
        }
        p->context = (struct kwf_context){sizeof(struct kwf_context),
                                          KWF_ABI_MAJOR,
                                          KWF_ABI_MINOR,
                                          process_role,
                                          (uintptr_t)loader_module,
                                          (uintptr_t)p->module,
                                          (uintptr_t)image,
                                          image_size,
                                          (const uint16_t *)runtime_root_directory,
                                          (const uint16_t *)plugin_directory,
                                          &host,
                                          p};
        prepare_result = p->api->prepare(&p->context);
        if (prepare_result != KWF_OK) {
            discard_preparation(i + 1);
            log_code(p, "prepare rejected; skipped", prepare_result);
            continue;
        }
        p->state = 1;
        runtime_log_message(p, "prepared");
    }
    runtime_log_message(NULL, "all prepare callbacks complete; activation begins");
    startup_phase = 2;
    for (k = 0; k < planned_count; ++k) {
        uint32_t i = order[k], r;
        struct loaded_plugin *p = &loaded_plugins[i];
        if (p->state != 1) {
            startup_status = KWF_ERROR;
            continue;
        }
        if (!dependencies_ready(i, 2)) {
            runtime_log_message(p, "dependency activation failed; skipped");
            startup_status = KWF_ERROR;
            continue;
        }
        if (p->api->activate() != KWF_OK) {
            for (r = patch_registry.count; r; --r) {
                struct kwf_patch *patch = &patch_registry.patches[r - 1];
                if (patch->owner == i + 1 && patch->written)
                    (void)restore_plugin_patch(p, patch->address, patch->length);
            }
            p->state = 0;
            startup_status = KWF_ERROR;
            runtime_log_message(p, "activation failed; owned patches restored where still matching");
        } else {
            p->state = 2;
            runtime_log_message(p, "active");
        }
    }
    startup_phase = 3;
    runtime_log_message(NULL, "startup complete; modules retained until process exit");
    return TRUE;
}
uint32_t KWF_CALL KwFrameworkInitialize(HMODULE loader, uint32_t role)
{
    /* Loader calls this once on its serialized startup path. A second role is
     * rejected rather than turning a rendered process into a headless process. */
    if (!loader || (role != KWF_ROLE_RENDERED && role != KWF_ROLE_HEADLESS))
        return KWF_ERROR;
    if (loader_module && (loader_module != loader || process_role != role))
        return KWF_ERROR;
    loader_module = loader;
    process_role = role;
    if (!InitOnceExecuteOnce(&runtime_init_once, initialize_plugin_runtime, NULL, NULL))
        return KWF_ERROR;
    return startup_status;
}
uint32_t KWF_CALL KwFrameworkStatus(void)
{
    return startup_status;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(module);
    return TRUE;
}
