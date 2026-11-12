/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_RESOLVER_H
#define KWENGINE_RESOLVER_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Reference addresses identify bindings, never candidate locations. Production
 * resolves them from instructions, RTTI and imports before installing hooks.
 * Standalone ABI fixtures retain their synthetic reference image by default. */
struct kwengine_resolution {
    uint32_t image_size, resolved, expected, missing_reference;
};
#ifdef KWENGINE_USE_RUNTIME_RESOLVER
#ifdef __cplusplus
extern "C" {
#endif
int kwengine_resolve(const void *image, size_t available, struct kwengine_resolution *status);
uintptr_t kwengine_va(uintptr_t reference);
uint32_t kwengine_rva(uint32_t reference);
int kwengine_memcmp(const void *a, const void *b, size_t size);
void kwengine_normalize_code(uintptr_t address, void *bytes, size_t size);
void kwengine_reset(void);
int kwengine_is_code(uintptr_t address);
int kwengine_is_data(uintptr_t address, size_t size);
/* These require a COMPLETE resolver cache. A readable/executable page or a
 * partially resolved image is not permission to install a hook. Patch-site
 * validation checks the catalog's masked prefix, not instruction relocation;
 * each installer must still prove complete stolen instructions and its ABI. */
int kwengine_is_ready(void);
int kwengine_code_span(uintptr_t address, size_t size);
int kwengine_patch_site_valid(uintptr_t address, size_t size);
typedef int (*kwengine_read_fn)(void *, uint32_t, void *, size_t);
int kwengine_chat_filter_valid(kwengine_read_fn read, void *context);
int kwengine_default_dependency(kwengine_read_fn read, void *context, uint32_t *value);
#ifdef __cplusplus
}
#endif
#else
static inline uintptr_t kwengine_va(uintptr_t value)
{
    return value;
}
static inline uint32_t kwengine_rva(uint32_t value)
{
    return value;
}
static inline int kwengine_memcmp(const void *a, const void *b, size_t n)
{
    return memcmp(a, b, n);
}
#endif
#endif
