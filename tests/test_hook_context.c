/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#include <kwengine/hook_context.h>
#include <assert.h>
#include <stdio.h>
static DWORD error_value;
DWORD GetLastError(void)
{
    return error_value;
}
void SetLastError(DWORD value)
{
    error_value = value;
}
int main(void)
{
    uint16_t original_cw, saved_cw, other_cw, actual_cw;
    uint32_t original_mxcsr, saved_mxcsr, other_mxcsr, actual_mxcsr;
    __asm__ volatile("fnstcw %0\n\tstmxcsr %1" : "=m"(original_cw), "=m"(original_mxcsr));
    saved_cw = (uint16_t)((original_cw & ~0x0c00U) | 0x0400U);
    other_cw = (uint16_t)((original_cw & ~0x0c00U) | 0x0c00U);
    saved_mxcsr = (original_mxcsr & ~0x6000U) | 0x2000U;
    other_mxcsr = (original_mxcsr & ~0x6000U) | 0x6000U;
    _Alignas(16) unsigned char storage[sizeof(struct kwengine_hook_context) + 32];
    for (unsigned offset = 0; offset < 16; offset += 4) {
        struct kwengine_hook_context *context = (struct kwengine_hook_context *)(storage + offset);
        __asm__ volatile("fldcw %0\n\tldmxcsr %1\n\tfld1" ::"m"(saved_cw), "m"(saved_mxcsr) : "st", "memory");
        error_value = 123;
        kwengine_hook_context_save(context);
        __asm__ volatile("fstp %%st(0)\n\tfldcw %0\n\tldmxcsr %1" ::"m"(other_cw), "m"(other_mxcsr) : "st", "memory");
        error_value = 456;
        kwengine_hook_context_restore(context);
        double value;
        __asm__ volatile("fnstcw %0\n\tstmxcsr %1\n\tfstpl %2" : "=m"(actual_cw), "=m"(actual_mxcsr), "=m"(value)::"st", "memory");
        assert(actual_cw == saved_cw && actual_mxcsr == saved_mxcsr && error_value == 123 && value == 1.0);
    }
    __asm__ volatile("fldcw %0\n\tldmxcsr %1" ::"m"(original_cw), "m"(original_mxcsr) : "memory");
    puts("PASS shared x86 hook context: x87 stack/control, MXCSR, LastError and all four stack alignments");
}
