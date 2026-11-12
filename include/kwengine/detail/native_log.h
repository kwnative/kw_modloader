/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_NATIVE_LOG_H
#define KWENGINE_NATIVE_LOG_H

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <kwengine/native_status.h>
#include <string.h>
#include <wchar.h>

/* Bounded diagnostic events only, never message contents or per-frame I/O.
 * Keep beside the actual loaded module, so failures can be inspected without
 * attaching a debugger or selecting a Wine debug channel. */
static inline void kwengine_log_line(HMODULE module, const char *line, size_t count)
{
    DWORD saved_error = GetLastError(), written;
    wchar_t path[MAX_PATH], *separator;
    DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    HANDLE file;
    if (!length || length >= MAX_PATH)
        goto done;
    separator = wcsrchr(path, L'\\');
    if (!separator || (size_t)(separator + 1 - path) + sizeof(L"kw_native_hooks.log") / sizeof(wchar_t) > MAX_PATH)
        goto done;
    memcpy(separator + 1, L"kw_native_hooks.log", sizeof(L"kw_native_hooks.log"));
    file =
        CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        goto done;
    if (count > 0 && count < 384U)
        WriteFile(file, line, (DWORD)count, &written, NULL);
    CloseHandle(file);
done:
    SetLastError(saved_error);
}
static inline void kwengine_log_event(HMODULE module, const char *stage, uint32_t result, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    DWORD saved_error = GetLastError();
    char line[384];
    int count = snprintf(line, sizeof(line), "[native-hooks] pid=%lu tick=%lu stage=%.64s result=%lu a=%08lx b=%08lx c=%08lx d=%08lx\r\n",
                         (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount(), stage, (unsigned long)result, (unsigned long)a,
                         (unsigned long)b, (unsigned long)c, (unsigned long)d);
    if (count > 0 && (size_t)count < sizeof(line))
        kwengine_log_line(module, line, (size_t)count);
    SetLastError(saved_error);
}
static inline void kwengine_log_build(HMODULE module, const struct kwengine_native_status *status)
{
    DWORD saved_error = GetLastError();
    char line[384];
    int count = snprintf(
        line, sizeof(line),
        "[native-hooks] pid=%lu stage=host-build state=%lu result=%lu image_size=%08lx signatures=%lu/%lu sha256=%.64s profile=%.31s\r\n",
        (unsigned long)GetCurrentProcessId(), (unsigned long)status->state, (unsigned long)status->result, (unsigned long)status->host_image_size,
        (unsigned long)status->signatures_matched, (unsigned long)status->signatures_expected, status->host_sha256, status->profile_id);
    if (count > 0 && (size_t)count < sizeof(line))
        kwengine_log_line(module, line, (size_t)count);
    SetLastError(saved_error);
}
#endif
#endif
