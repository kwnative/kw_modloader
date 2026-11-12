/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_HOOK_CONTEXT_H
#define KWENGINE_HOOK_CONTEXT_H
#include <windows.h>
#include <stdint.h>
/* Native x86 hook-call context. The extra 15 bytes permit 16-byte FXSAVE
 * alignment even on the engine's four-byte-aligned stack. Not thread offload. */
struct kwengine_hook_context {
    unsigned char bytes[527];
    DWORD error;
};
static inline void kwengine_hook_context_save(struct kwengine_hook_context *context)
{
    void *aligned = (void *)(((uintptr_t)context->bytes + 15U) & ~(uintptr_t)15U);
    __asm__ __volatile__("fxsave (%0)" ::"r"(aligned) : "memory");
    context->error = GetLastError();
}
static inline void kwengine_hook_context_restore(const struct kwengine_hook_context *context)
{
    const void *aligned = (const void *)(((uintptr_t)context->bytes + 15U) & ~(uintptr_t)15U);
    SetLastError(context->error);
    __asm__ __volatile__("fxrstor (%0)" ::"r"(aligned) : "memory");
}
#endif
