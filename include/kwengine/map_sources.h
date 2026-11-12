/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_MAP_SOURCES_H
#define KWENGINE_MAP_SOURCES_H
#include <kwengine/engine_address.h>

/* Native source locations only, not a public-map/disclosure policy. Consumers
 * must guard engine owner thread, source layout, map, player and episode.
 * These are process-private addresses; never copy this structure into IPC.
 * Snapshot methods are mutating native functions, NOT queries exposed here.
 */
enum kwengine_resource_property {
    KWENGINE_RESOURCE_RADIUS,
    KWENGINE_RESOURCE_MAXIMUM_MONEY,
    KWENGINE_RESOURCE_STARTING_MONEY,
    KWENGINE_RESOURCE_LIFETIME_MONEY,
    KWENGINE_RESOURCE_GROWTH,
    KWENGINE_RESOURCE_RESOLUTION,
    KWENGINE_RESOURCE_CLUSTER_SCALE,
    KWENGINE_RESOURCE_CLUSTER_POWER,
    KWENGINE_RESOURCE_PROPERTY_COUNT
};
struct kwengine_map_source_bindings {
    uint32_t authored_holder, authored_vtable;
    uint32_t ghost_manager_global, ghost_manager_vtable, ghost_vtable;
    uint32_t resource_keys[KWENGINE_RESOURCE_PROPERTY_COUNT];
};
struct kwengine_ghost_hook_bindings {
    uint32_t allocate, snapshot, clear_player, clear_local;
};
struct kwengine_object_fog_bindings {
    uint32_t shroud_interface_vtable, hidden_when_fogged;
};
struct kwengine_initial_map_hook_bindings {
    uint32_t first_pass, second_pass, properties, ordinary_return, bridge_return;
    uint32_t partition_registration; /* Read-only native exclusion witness; not an installed hook. */
};

#ifdef KWENGINE_USE_RUNTIME_RESOLVER
/* Uses only the single shared resolver snapshot. No scanner, native calls,
 * key initialization, fixed-address fallback or dependency on another plugin.
 * Readiness/data-span checks do not themselves establish observation rights.
 */
static inline int kwengine_map_sources(struct kwengine_map_source_bindings *out)
{
    static const uint32_t key_references[KWENGINE_RESOURCE_PROPERTY_COUNT] = {0xb70508U, 0xb70510U, 0xb70518U, 0xb70520U,
                                                                              0xb70528U, 0xb70530U, 0xb70538U, 0xb70540U};
    struct kwengine_map_source_bindings sample;
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    memset(&sample, 0, sizeof(sample));
    if (!kwengine_is_ready())
        return 0;
    sample.authored_holder = (uint32_t)kwengine_va(0xb70660U);
    sample.authored_vtable = (uint32_t)kwengine_va(0xa50028U);
    sample.ghost_manager_global = (uint32_t)kwengine_va(0xbe7eecU);
    sample.ghost_manager_vtable = (uint32_t)kwengine_va(0xa9a99cU);
    sample.ghost_vtable = (uint32_t)kwengine_va(0xa9a8acU);
    if (!kwengine_is_data(sample.authored_holder, 4) || !kwengine_is_data(sample.authored_vtable, 4) ||
        !kwengine_is_data(sample.ghost_manager_global, 4) || !kwengine_is_data(sample.ghost_manager_vtable, 4) ||
        !kwengine_is_data(sample.ghost_vtable, 4))
        return 0;
    for (uint32_t i = 0; i < KWENGINE_RESOURCE_PROPERTY_COUNT; ++i) {
        sample.resource_keys[i] = (uint32_t)kwengine_va(key_references[i]);
        if (!kwengine_is_data(sample.resource_keys[i], 8))
            return 0;
    }
    *out = sample;
    return 1;
}
/* Exact native entry spans: complete x86 instructions, no relative operand in
 * the stolen prefixes. Discovery/byte checks are NOT patch ownership. Installers
 * must establish patch ownership/conflict checks and preserve native ABI/state.
 * Loader plugins use their patch broker; a legacy injected adapter's checked
 * direct installer is not implicitly broker-owned merely by using this API.
 * These callbacks remain game-thread lifecycle events, never worker queries.
 */
static inline int kwengine_ghost_hook_sources(struct kwengine_ghost_hook_bindings *out)
{
    struct kwengine_ghost_hook_bindings sample;
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!kwengine_is_ready())
        return 0;
    sample.allocate = (uint32_t)kwengine_va(0x782476U);
    sample.snapshot = (uint32_t)kwengine_va(0x7a462cU);
    sample.clear_player = (uint32_t)kwengine_va(0x7823b6U);
    sample.clear_local = (uint32_t)kwengine_va(0x782370U);
    if (!kwengine_patch_site_valid(sample.allocate, 7) || !kwengine_patch_site_valid(sample.snapshot, 8) ||
        !kwengine_patch_site_valid(sample.clear_player, 7) || !kwengine_patch_site_valid(sample.clear_local, 8))
        return 0;
    *out = sample;
    return 1;
}
/* Source addresses only; this helper makes NO native call. The predicate uses
 * x86 thiscall with secondary this=Object+0x64 and a player argument; true means
 * HIDE in fog. It is not the primary Object vtable's slot+0x14. A false result
 * is NOT initial-public-map permission: no partition also returns false, and
 * ownership/seen-state are inputs. Initial provenance and map/episode/player/
 * owner-thread guards must be established separately before any native use.
 * Never call it on a worker or use it to read hidden current object payload.
 */
static inline int kwengine_object_fog_sources(struct kwengine_object_fog_bindings *out)
{
    struct kwengine_object_fog_bindings sample;
    uint32_t target;
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!kwengine_is_ready())
        return 0;
    sample.shroud_interface_vtable = (uint32_t)kwengine_va(0xa88dacU);
    sample.hidden_when_fogged = (uint32_t)kwengine_va(0x60b93fU);
    if (!kwengine_is_data(sample.shroud_interface_vtable, 0x18) || !kwengine_patch_site_valid(sample.hidden_when_fogged, 33))
        return 0;
    memcpy(&target, (const void *)(uintptr_t)(sample.shroud_interface_vtable + 0x14), sizeof(target));
    if (target != sample.hidden_when_fogged)
        return 0;
    *out = sample;
    return 1;
}
/* Complete x86 entry prefixes; first/second passes use six stolen bytes and
 * properties uses eight, with no relative operand. Return sites are provenance
 * witnesses only: never hook their borrowed frames or treat them as entries.
 * A pass may skip templates or be used in restore modes; binding does not prove
 * successful initial creation or grant public visibility. No native calls.
 */
static inline int kwengine_initial_map_hook_sources(struct kwengine_initial_map_hook_bindings *out)
{
    struct kwengine_initial_map_hook_bindings sample;
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!kwengine_is_ready())
        return 0;
    sample.first_pass = (uint32_t)kwengine_va(0x4f24e2U);
    sample.second_pass = (uint32_t)kwengine_va(0x4f26ceU);
    sample.properties = (uint32_t)kwengine_va(0x67f082U);
    sample.ordinary_return = (uint32_t)kwengine_va(0x4e5e41U);
    sample.bridge_return = (uint32_t)kwengine_va(0x4f2662U);
    sample.partition_registration = (uint32_t)kwengine_va(0x60b121U);
    if (!kwengine_patch_site_valid(sample.first_pass, 6) || !kwengine_patch_site_valid(sample.second_pass, 6) ||
        !kwengine_patch_site_valid(sample.properties, 8) || !kwengine_patch_site_valid(sample.partition_registration, 31))
        return 0;
    const uint32_t returns[2] = {sample.ordinary_return, sample.bridge_return};
    for (uint32_t i = 0; i < 2; ++i) {
        int32_t displacement;
        if (returns[i] < 5 || !kwengine_code_span(returns[i] - 5, 5) || *(const unsigned char *)(uintptr_t)(returns[i] - 5) != 0xe8)
            return 0;
        memcpy(&displacement, (const void *)(uintptr_t)(returns[i] - 4), 4);
        if ((uint32_t)(returns[i] + displacement) != sample.properties)
            return 0;
    }
    *out = sample;
    return 1;
}
#endif
#endif
