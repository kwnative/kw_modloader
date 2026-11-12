/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_BEACON_SOURCES_H
#define KWENGINE_BEACON_SOURCES_H
#include <kwengine/engine_address.h>
/* Native callsite/ABI authority only; does not prove a particular recipient's
 * permission or beacon lifecycle. Plugin policy remains outside the SDK. */
struct kwengine_beacon_sources {
    uintptr_t notification, caller;
};
static inline int kwengine_beacon_sources_read(struct kwengine_beacon_sources *out)
{
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
#ifdef KWENGINE_USE_RUNTIME_RESOLVER
    unsigned char call[5], body[48];
    uint32_t relative;
    uintptr_t site, target;
    static const unsigned char prefix[6] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
    if (!kwengine_is_ready() || !(site = kwengine_va(0x51b684U)) || !(target = kwengine_va(0x74d829U)) ||
        !kwengine_patch_site_valid(site, sizeof(call)) || !kwengine_patch_site_valid(target, sizeof(prefix)) ||
        !kwengine_code_span(target, sizeof(body)))
        return 0;
    memcpy(call, (const void *)site, sizeof(call));
    memcpy(body, (const void *)target, sizeof(body));
    memcpy(&relative, call + 1, sizeof(relative));
    if (call[0] != 0xe8 || (uint32_t)(site + 5 + relative) != target || memcmp(body, prefix, sizeof(prefix)) ||
        body[46] != 0xc9 || body[47] != 0xc3)
        return 0;
    out->notification = target;
    out->caller = site + 5;
    return 1;
#else
    return 0; /* A synthetic reference address is not hook authority. */
#endif
}
#endif
