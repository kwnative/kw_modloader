/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWF_PATCH_QUERY_H
#define KWF_PATCH_QUERY_H
#include <kwf/plugin.h>
/* Read-only broker metadata. A reservation is not permission to write another
 * owner's bytes. An unknown/unbrokered patch is never represented as trusted. */
#define KWF_PATCH_QUERY_SERVICE "kwf.patch-query"
struct kwf_patch_info {
    uint32_t size, length, written, provider_state;
    uintptr_t address;
    const char *provider_id;
};
struct kwf_patch_query {
    uint32_t size, version;
    /* Startup/engine owner thread only. UNSUPPORTED means no overlapping
     * reservation. CONFLICT means multiple overlaps or insufficient capacity. */
    uint32_t(KWF_CALL *find)(void *requester, uintptr_t address, uint32_t length, struct kwf_patch_info *info);
    uint32_t(KWF_CALL *original)(void *requester, uintptr_t address, uint32_t length, void *bytes);
};
#endif
