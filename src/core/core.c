/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#include "core.h"
#include <string.h>
#include <stdlib.h>
static int identifier(const char *s)
{
    size_t n = 0;
    if (!s || !*s)
        return 0;
    for (; *s; ++s, ++n)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-' || *s == '.'))
            return 0;
    return n < KWF_ID_SIZE;
}
static char *trim(char *s)
{
    char *end;
    while (*s == ' ' || *s == '\t')
        ++s;
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        --end;
    *end = 0;
    return s;
}
int kwf_manifest_parse(const char *text, size_t size, struct kwf_manifest *out)
{
    char buffer[KWF_MANIFEST_LIMIT + 1], *line;
    uint32_t fields = 0;
    if (!text || !out || !size || size > KWF_MANIFEST_LIMIT || memchr(text, 0, size))
        return 0;
    memset(out, 0, sizeof(*out));
    memcpy(buffer, text, size);
    buffer[size] = 0;
    line = buffer;
    while (line) {
        char *next = strchr(line, '\n'), *key, *value, *equal;
        uint32_t bit;
        if (next)
            *next++ = 0;
        key = trim(line);
        line = next;
        if (!*key || *key == '#' || *key == ';')
            continue;
        equal = strchr(key, '=');
        if (!equal)
            return 0;
        *equal = 0;
        value = trim(equal + 1);
        key = trim(key);
        if (!strcmp(key, "id")) {
            bit = 1;
            if (!identifier(value))
                return 0;
            strcpy(out->id, value);
        } else if (!strcmp(key, "library")) {
            size_t n = strlen(value);
            bit = 2;
            if (n < 5 || n >= sizeof(out->library) || !identifier(value) || strcmp(value + n - 4, ".dll") || strstr(value, ".."))
                return 0;
            strcpy(out->library, value);
        } else if (!strcmp(key, "enabled")) {
            bit = 4;
            if (strcmp(value, "0") && strcmp(value, "1"))
                return 0;
            out->enabled = (uint32_t)(*value == '1');
        } else if (!strcmp(key, "roles")) {
            bit = 8;
            if (!strcmp(value, "rendered"))
                out->roles = KWF_ROLE_RENDERED;
            else if (!strcmp(value, "headless"))
                out->roles = KWF_ROLE_HEADLESS;
            else if (!strcmp(value, "any"))
                out->roles = KWF_ROLE_RENDERED | KWF_ROLE_HEADLESS;
            else
                return 0;
        } else if (!strcmp(key, "abi")) {
            bit = 16;
            if (strcmp(value, "1"))
                return 0;
        } else if (!strcmp(key, "requires") || !strcmp(key, "after")) {
            int optional = !strcmp(key, "after");
            char (*names)[KWF_ID_SIZE] = optional ? out->after : out->required_plugins;
            uint32_t *count = optional ? &out->after_count : &out->dependency_count;
            bit = optional ? 64 : 32;
            while (*value) {
                char *comma = strchr(value, ',');
                uint32_t i;
                if (comma)
                    *comma++ = 0;
                value = trim(value);
                if (!identifier(value) || *count == KWF_MAX_DEPENDENCIES)
                    return 0;
                for (i = 0; i < *count; ++i)
                    if (!strcmp(names[i], value))
                        return 0;
                strcpy(names[(*count)++], value);
                if (!comma)
                    break;
                value = trim(comma);
                if (!*value)
                    return 0;
            }
        } else
            return 0;
        if (fields & bit)
            return 0;
        fields |= bit;
    }
    return (fields & 31U) == 31U;
}
uint32_t kwf_plan_available(const struct kwf_manifest *items, uint32_t count, uint32_t *order)
{
    uint32_t emitted[KWF_MAX_PLUGINS] = {0}, invalid[KWF_MAX_PLUGINS] = {0}, available[KWF_MAX_PLUGINS] = {0}, step, i, j, d;
    if (!items || !order || count > KWF_MAX_PLUGINS)
        return 0;
    for (i = 0; i < count; ++i) {
        if (!items[i].enabled)
            invalid[i] = 1;
        for (j = 0; j < i; ++j)
            if (!strcmp(items[i].id, items[j].id) || !strcmp(items[i].library, items[j].library))
                invalid[i] = invalid[j] = 1;
    }
    /* Optional ordering must not turn an absent or required-dependency-invalid
     * provider into a mandatory dependency. Find viable required components. */
    for (step = 0; step < count; ++step) {
        int progress = 0;
        for (i = 0; i < count; ++i) {
            int ready = 1;
            if (invalid[i] || available[i])
                continue;
            for (d = 0; d < items[i].dependency_count; ++d) {
                for (j = 0; j < count; ++j)
                    if (!strcmp(items[j].id, items[i].required_plugins[d]))
                        break;
                if (j == count || !available[j]) {
                    ready = 0;
                    break;
                }
            }
            if (ready) {
                available[i] = 1;
                progress = 1;
            }
        }
        if (!progress)
            break;
    }
    for (i = 0; i < count; ++i)
        if (!available[i])
            invalid[i] = 1;
    for (step = 0; step < count; ++step) {
        uint32_t best = count;
        for (i = 0; i < count; ++i) {
            int ready = 1;
            if (emitted[i] || invalid[i])
                continue;
            for (d = 0; d < items[i].dependency_count; ++d) {
                for (j = 0; j < count; ++j)
                    if (!strcmp(items[j].id, items[i].required_plugins[d]))
                        break;
                if (j == count || !emitted[j]) {
                    ready = 0;
                    break;
                }
            }
            for (d = 0; ready && d < items[i].after_count; ++d) {
                for (j = 0; j < count; ++j)
                    if (!strcmp(items[j].id, items[i].after[d]))
                        break;
                if (j < count && !invalid[j] && !emitted[j])
                    ready = 0;
            }
            if (ready && (best == count || strcmp(items[i].id, items[best].id) < 0))
                best = i;
        }
        if (best == count)
            return step;
        order[step] = best;
        emitted[best] = 1;
    }
    return count;
}
int kwf_plan(const struct kwf_manifest *items, uint32_t count, uint32_t *order)
{
    return items && order && count <= KWF_MAX_PLUGINS && kwf_plan_available(items, count, order) == count;
}
int kwf_descriptor_valid(const struct kwf_manifest *m, const struct kwf_plugin *p, uint32_t role)
{
    return m && p && p->size >= sizeof(*p) && p->abi_major == KWF_ABI_MAJOR && p->abi_minor <= KWF_ABI_MINOR && p->id && !strcmp(m->id, p->id) &&
           (p->roles & role) && !(p->required_capabilities & ~KWF_CAP_PATCHES) && p->prepare && p->activate;
}
uint32_t kwf_reserve(struct kwf_registry *r, uint32_t owner, uintptr_t address, uint32_t length)
{
    uint32_t i;
    struct kwf_patch *p;
    if (!r || !owner || !address || !length || length > KWF_MAX_PATCH_BYTES || address > UINTPTR_MAX - length)
        return KWF_ERROR;
    for (i = 0; i < r->count; ++i) {
        p = &r->patches[i];
        if (p->owner == owner && p->address == address && p->length == length)
            return KWF_OK;
        if (address < p->address + p->length && p->address < address + length)
            return KWF_CONFLICT;
    }
    if (r->count == UINT32_MAX)
        return KWF_ERROR;
    if (r->count == r->capacity) {
        /* Allocation granularity, not a hook-count policy or game limit.
         * Keep the original registry untouched if allocation fails. */
        uint32_t capacity = r->capacity ? r->capacity : 32U;
        if (r->capacity)
            capacity = capacity > UINT32_MAX / 2U ? UINT32_MAX : capacity * 2U;
        if ((uint64_t)capacity * sizeof(*p) > SIZE_MAX)
            return KWF_ERROR;
        struct kwf_patch *grown = realloc(r->patches, (size_t)capacity * sizeof(*p));
        if (!grown)
            return KWF_ERROR;
        r->patches = grown;
        r->capacity = capacity;
    }
    p = &r->patches[r->count++];
    memset(p, 0, sizeof(*p));
    p->address = address;
    p->length = length;
    p->owner = owner;
    return KWF_OK;
}
struct kwf_patch *kwf_patch_find(struct kwf_registry *r, uint32_t owner, uintptr_t address, uint32_t length)
{
    uint32_t i;
    if (!r)
        return NULL;
    for (i = 0; i < r->count; ++i)
        if (r->patches[i].owner == owner && r->patches[i].address == address && r->patches[i].length == length)
            return &r->patches[i];
    return NULL;
}
void kwf_registry_dispose(struct kwf_registry *r)
{
    if (!r)
        return;
    free(r->patches);
    memset(r, 0, sizeof(*r));
}
