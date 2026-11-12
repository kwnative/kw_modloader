/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
/* Bind the optional engine component from the loader's absolute directory.
 * An implicit import from plugins/ would depend on DLL search order or on the
 * AI adapter having loaded it first. This client never embeds a resolver. */
#define KWENGINE_CLIENT_FUNCTIONS(X)                                                                                                                 \
    X(resolve, int, (const void *, size_t, struct kwengine_resolution *))                                                                            \
    X(va, uintptr_t, (uintptr_t))                                                                                                                    \
    X(rva, uint32_t, (uint32_t))                                                                                                                     \
    X(memcmp, int, (const void *, const void *, size_t))                                                                                             \
    X(normalize_code, void, (uintptr_t, void *, size_t))                                                                                             \
    X(is_ready, int, (void))                                                                                                                         \
    X(is_code, int, (uintptr_t))                                                                                                                     \
    X(is_data, int, (uintptr_t, size_t))                                                                                                             \
    X(code_span, int, (uintptr_t, size_t))                                                                                                           \
    X(patch_site_valid, int, (uintptr_t, size_t))
static struct {
#define KWENGINE_CLIENT_FIELD(name, result, args) result(*fn_##name) args;
    KWENGINE_CLIENT_FUNCTIONS(KWENGINE_CLIENT_FIELD)
#undef KWENGINE_CLIENT_FIELD
} kwengine_client;
static int kwengine_client_bind(const struct kwf_context *context)
{
    wchar_t path[MAX_PATH];
    size_t length = wcslen((const wchar_t *)context->root_directory);
    if (length + sizeof(L"\\kw_engine.dll") / sizeof(wchar_t) > MAX_PATH)
        return 0;
    memcpy(path, context->root_directory, length * sizeof(wchar_t));
    memcpy(path + length, L"\\kw_engine.dll", sizeof(L"\\kw_engine.dll"));
    HMODULE module = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module)
        return 0;
#define KWENGINE_CLIENT_BIND(name, result, args)                                                                                                     \
    kwengine_client.fn_##name = (__typeof__(kwengine_client.fn_##name))(uintptr_t)GetProcAddress(module, "kwengine_" #name);                         \
    if (!kwengine_client.fn_##name)                                                                                                                  \
        return 0;
    KWENGINE_CLIENT_FUNCTIONS(KWENGINE_CLIENT_BIND)
#undef KWENGINE_CLIENT_BIND
    return 1;
}
#define kwengine_resolve kwengine_client.fn_resolve
#define kwengine_va kwengine_client.fn_va
#define kwengine_rva kwengine_client.fn_rva
#define kwengine_memcmp kwengine_client.fn_memcmp
#define kwengine_normalize_code kwengine_client.fn_normalize_code
#define kwengine_is_ready kwengine_client.fn_is_ready
#define kwengine_is_code kwengine_client.fn_is_code
#define kwengine_is_data kwengine_client.fn_is_data
#define kwengine_code_span kwengine_client.fn_code_span
#define kwengine_patch_site_valid kwengine_client.fn_patch_site_valid
