/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdint.h>
#include <wchar.h>
#include <kwf/plugin.h>
#include <kwf/loader.h>
typedef HRESULT(WINAPI *direct_input_factory_fn)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
typedef HRESULT(WINAPI *com_noarg_fn)(void);
typedef HRESULT(WINAPI *com_class_fn)(REFCLSID, REFIID, LPVOID *);
typedef uint32_t(KWF_CALL *runtime_initialize_fn)(HMODULE, uint32_t);
static HMODULE loader_module, system_dinput_module, runtime_module;
static INIT_ONCE system_once = INIT_ONCE_STATIC_INIT, runtime_once = INIT_ONCE_STATIC_INIT;
static uint32_t configured_process_role = KWF_ROLE_RENDERED, runtime_start_result = KWF_BAD_PHASE;
static BOOL CALLBACK load_system_dinput(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    wchar_t path[MAX_PATH];
    UINT n;
    (void)once;
    (void)parameter;
    (void)context;
    n = GetSystemDirectoryW(path, MAX_PATH);
    if (n && n + 13 < MAX_PATH) {
        memcpy(path + n, L"\\dinput8.dll", sizeof(L"\\dinput8.dll"));
        system_dinput_module = LoadLibraryW(path);
        if (system_dinput_module == loader_module)
            system_dinput_module = NULL;
    }
    return TRUE;
}
static BOOL CALLBACK initialize_framework_runtime(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    wchar_t path[MAX_PATH], *end;
    DWORD n;
    runtime_initialize_fn start;
    (void)once;
    (void)parameter;
    (void)context;
    runtime_start_result = KWF_ERROR;
    n = GetModuleFileNameW(loader_module, path, MAX_PATH);
    if (!n || n >= MAX_PATH || !(end = wcsrchr(path, L'\\')) || (size_t)(end + 1 - path) + 15 > MAX_PATH)
        return TRUE;
    memcpy(end + 1, L"kw_runtime.dll", sizeof(L"kw_runtime.dll"));
    runtime_module = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!runtime_module) {
        OutputDebugStringA("kw_framework: runtime load failed; system DirectInput remains available\n");
        return TRUE;
    }
    start = (runtime_initialize_fn)(uintptr_t)GetProcAddress(runtime_module, "KwFrameworkInitialize");
    if (start)
        runtime_start_result = start(loader_module, configured_process_role);
    return TRUE;
}
KWF_EXPORT uint32_t KWF_CALL KwFrameworkStart(uint32_t requested_process_role)
{
    if (requested_process_role != KWF_ROLE_RENDERED && requested_process_role != KWF_ROLE_HEADLESS)
        return KWF_ERROR;
    /* Startup-only API. The owning injector or factory calls on one thread. */
    if (runtime_start_result != KWF_BAD_PHASE && requested_process_role != configured_process_role)
        return KWF_ERROR;
    configured_process_role = requested_process_role;
    InitOnceExecuteOnce(&runtime_once, initialize_framework_runtime, NULL, NULL);
    return runtime_start_result;
}
KWF_EXPORT uint32_t KWF_CALL KwFrameworkLoaderStatus(void)
{
    return runtime_start_result;
}
KWF_EXPORT uint32_t __stdcall KwFrameworkStartInjected(void *role)
{
    return KwFrameworkStart((uint32_t)(uintptr_t)role);
}
KWF_EXPORT HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, REFIID iid, LPVOID *output, LPUNKNOWN outer)
{
    direct_input_factory_fn factory;
    InitOnceExecuteOnce(&system_once, load_system_dinput, NULL, NULL);
    if (!system_dinput_module)
        return E_FAIL;
    factory = (direct_input_factory_fn)(uintptr_t)GetProcAddress(system_dinput_module, "DirectInput8Create");
    if (!factory)
        return E_FAIL;
    /* A managed injector may already have selected the bot role. */
    if (runtime_start_result == KWF_BAD_PHASE)
        (void)KwFrameworkStart(KWF_ROLE_RENDERED);
    return factory(instance, version, iid, output, outer);
}
KWF_EXPORT HRESULT WINAPI DllCanUnloadNow(void)
{
    com_noarg_fn call;
    InitOnceExecuteOnce(&system_once, load_system_dinput, NULL, NULL);
    call = system_dinput_module ? (com_noarg_fn)(uintptr_t)GetProcAddress(system_dinput_module, "DllCanUnloadNow") : NULL;
    return call ? call() : S_FALSE;
}
KWF_EXPORT HRESULT WINAPI DllGetClassObject(REFCLSID cls, REFIID iid, LPVOID *out)
{
    com_class_fn call;
    InitOnceExecuteOnce(&system_once, load_system_dinput, NULL, NULL);
    call = system_dinput_module ? (com_class_fn)(uintptr_t)GetProcAddress(system_dinput_module, "DllGetClassObject") : NULL;
    return call ? call(cls, iid, out) : CLASS_E_CLASSNOTAVAILABLE;
}
KWF_EXPORT HRESULT WINAPI DllRegisterServer(void)
{
    com_noarg_fn call;
    InitOnceExecuteOnce(&system_once, load_system_dinput, NULL, NULL);
    call = system_dinput_module ? (com_noarg_fn)(uintptr_t)GetProcAddress(system_dinput_module, "DllRegisterServer") : NULL;
    return call ? call() : E_NOTIMPL;
}
KWF_EXPORT HRESULT WINAPI DllUnregisterServer(void)
{
    com_noarg_fn call;
    InitOnceExecuteOnce(&system_once, load_system_dinput, NULL, NULL);
    call = system_dinput_module ? (com_noarg_fn)(uintptr_t)GetProcAddress(system_dinput_module, "DllUnregisterServer") : NULL;
    return call ? call() : E_NOTIMPL;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        loader_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
