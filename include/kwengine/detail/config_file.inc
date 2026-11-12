/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
/* Generic bounded file loading; schemas and fallback policy are caller-owned.
 * A readable invalid dedicated INI never silently falls back to another file. */
typedef int (*kwengine_config_file_parser)(const void *, size_t, void *, int, size_t *);
static int kwengine_config_load(const struct kwf_context *context, const wchar_t *preferred, const wchar_t *fallback, void *config,
                                kwengine_config_file_parser parse)
{
    wchar_t path[MAX_PATH];
    char data[16384];
    const wchar_t *root = (const wchar_t *)context->root_directory;
    size_t length = wcslen(root), line = 0;
    DWORD size, got;
    HANDLE file = INVALID_HANDLE_VALUE;
    int legacy = 0;
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        const wchar_t *name = attempt ? fallback : preferred;
        if (!name)
            continue;
        if (length + wcslen(name) >= MAX_PATH)
            return 0;
        memcpy(path, root, length * sizeof(wchar_t));
        memcpy(path + length, name, (wcslen(name) + 1) * sizeof(wchar_t));
        file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            legacy = attempt != 0;
            break;
        }
        if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND)
            return 0;
    }
    if (file == INVALID_HANDLE_VALUE)
        return parse(NULL, 0, config, 0, &line);
    size = GetFileSize(file, NULL);
    int ok = size <= sizeof(data) && ReadFile(file, data, size, &got, NULL) && got == size && parse(data, size, config, legacy, &line);
    CloseHandle(file);
    return ok;
}
