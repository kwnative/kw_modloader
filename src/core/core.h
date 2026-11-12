/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#ifndef KWF_CORE_H
#define KWF_CORE_H
#include <kwf/plugin.h>
#define KWF_MAX_PLUGINS 32U
#define KWF_MAX_DEPENDENCIES 8U
#define KWF_ID_SIZE 64U
#define KWF_MANIFEST_LIMIT 4096U
#define KWF_MAX_PATCH_BYTES 64U
struct kwf_manifest {
    char id[KWF_ID_SIZE], library[96];
    char required_plugins[KWF_MAX_DEPENDENCIES][KWF_ID_SIZE];
    char after[KWF_MAX_DEPENDENCIES][KWF_ID_SIZE];
    uint32_t enabled, roles, dependency_count, after_count;
};
int kwf_manifest_parse(const char *text, size_t size, struct kwf_manifest *out);
/* Returns false for missing/disabled dependencies, duplicate IDs/libraries or
 * cycles. Caller prefilters roles; ties use ID order, never filesystem order. */
int kwf_plan(const struct kwf_manifest *items, uint32_t count, uint32_t *order);
/* Plan independent valid components even when another component is invalid.
 * Returns the number emitted; unplanned plugins/dependants must not be loaded. */
uint32_t kwf_plan_available(const struct kwf_manifest *items, uint32_t count, uint32_t *order);
int kwf_descriptor_valid(const struct kwf_manifest *manifest, const struct kwf_plugin *plugin, uint32_t role);
struct kwf_patch {
    uintptr_t address;
    uint32_t length, owner, written;
    unsigned char original[KWF_MAX_PATCH_BYTES], replacement[KWF_MAX_PATCH_BYTES];
};
struct kwf_registry {
    uint32_t count, capacity;
    struct kwf_patch *patches;
};
/* Internal metadata only; no public plugin ABI change. Reserve is serialized
 * during startup preparation. Lookup pointers are borrowed: do not retain them
 * across growth, owner rollback/compaction or disposal. Activation starts only
 * after all preparation ends; public broker calls do not expose these pointers. */
uint32_t kwf_reserve(struct kwf_registry *r, uint32_t owner, uintptr_t address, uint32_t length);
struct kwf_patch *kwf_patch_find(struct kwf_registry *r, uint32_t owner, uintptr_t address, uint32_t length);
/* Caller must restore all written patches and stop registry users first. */
void kwf_registry_dispose(struct kwf_registry *r);
#endif
