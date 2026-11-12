/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
/* Native memory reads shared by rendered plugins; raw copies stay raw. */
static inline int readable_memory(const void *pointer, size_t length)
{
    uintptr_t at = (uintptr_t)pointer, end;
    if (!pointer || !length || length > UINTPTR_MAX - at)
        return 0;
    end = at + length;
    while (at < end) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((const void *)at, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return 0;
        DWORD access = info.Protect & 0xffU;
        if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY && access != PAGE_EXECUTE_READ &&
            access != PAGE_EXECUTE_READWRITE && access != PAGE_EXECUTE_WRITECOPY)
            return 0;
        uintptr_t next = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next <= at)
            return 0;
        at = next;
    }
    return 1;
}
static inline int read_process_raw(const void *address, void *output, SIZE_T length)
{
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, output, length, &copied) && copied == length;
}
static inline int read_process_value(const void *address, void *output, SIZE_T length)
{
    if (!read_process_raw(address, output, length))
        return 0;
    kwengine_normalize_code((uintptr_t)address, output, length);
    return 1;
}
