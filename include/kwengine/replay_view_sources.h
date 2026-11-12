/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_REPLAY_VIEW_SOURCES_H
#define KWENGINE_REPLAY_VIEW_SOURCES_H
#include <kwengine/engine_address.h>
/* Process-private native sources, not a live player/control grant. The setter
 * is the stock observer/replay viewpoint operation. It must never be replaced
 * by writing PlayerList::local or by treating a replay slot as an engine index.
 * Binding performs no native calls, writes, hooks or perspective selection.
 */
struct kwengine_replay_view_bindings {
    uint32_t select_observed_player, ghost_select_player;
};
#ifdef KWENGINE_USE_RUNTIME_RESOLVER
static inline int kwengine_replay_view_sources(struct kwengine_replay_view_bindings *out)
{
    struct kwengine_replay_view_bindings sample;
    uint32_t manager_type, method;
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!kwengine_is_ready())
        return 0;
    sample.select_observed_player = (uint32_t)kwengine_va(0x49f5b9U);
    sample.ghost_select_player = (uint32_t)kwengine_va(0x775834U);
    manager_type = (uint32_t)kwengine_va(0xa9a99cU);
    if (!kwengine_patch_site_valid(sample.select_observed_player, 6) || !kwengine_patch_site_valid(sample.ghost_select_player, 8) ||
        !kwengine_is_data(manager_type, 0x18))
        return 0;
    memcpy(&method, (const void *)(uintptr_t)(manager_type + 0x14), 4);
    if (method != sample.ghost_select_player)
        return 0;
    *out = sample;
    return 1;
}
#endif
#endif
