/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
/* KW-specific engine component. One immutable discovery snapshot per process;
 * all consumers import this DLL instead of embedding competing resolver caches. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define KWENGINE_USE_RUNTIME_RESOLVER
#define kwengine_resolve kw_engine_resolve_uncached
#define kwengine_reset kw_engine_private_reset
#include "engine_address.c"
#undef kwengine_resolve
#undef kwengine_reset
static INIT_ONCE resolution_once = INIT_ONCE_STATIC_INIT;
static struct kwengine_resolution cached_status;
static const void *cached_image;
static size_t cached_available;
static int cached_result;
struct resolve_args {
    const void *image;
    size_t available;
};
static BOOL CALLBACK resolve_once(PINIT_ONCE once, PVOID argument, PVOID *context)
{
    const struct resolve_args *args = argument;
    (void)once;
    (void)context;
    cached_image = args->image;
    cached_available = args->available;
    cached_result = kw_engine_resolve_uncached(args->image, args->available, &cached_status);
    return TRUE;
}
int kwengine_resolve(const void *image, size_t available, struct kwengine_resolution *status)
{
    struct resolve_args args = {image, available};
    if (!status || !InitOnceExecuteOnce(&resolution_once, resolve_once, &args, NULL))
        return 0;
    if (image != cached_image || available != cached_available) {
        memset(status, 0, sizeof(*status));
        return 0;
    }
    *status = cached_status;
    return cached_result;
}
