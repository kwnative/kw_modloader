/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_SELECTION_READER_H
#define KWENGINE_SELECTION_READER_H
#include <kwengine/engine_address.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Research diagnostic ONLY. Caller validates runtime bindings and native ABI.
 * Selected objects can include enemies; this is NOT an ownership/visibility
 * proof and MUST NOT feed the model. No engine calls or cached object pointers.
 * Two matching reads reduce races but do not establish atomicity (ABA possible).
 * Prototype bound: reject oversized groups instead of exporting a partial list. */
#define KWENGINE_SELECTION_LIMIT 128u
typedef int (*kwengine_selection_read_fn)(void *, uint32_t, void *, size_t);
struct kwengine_selection_sample {
    uint32_t frame, player, count, ids[KWENGINE_SELECTION_LIMIT];
};
struct kwengine_selection_internal {
    uint32_t logic, list, local, squad, vector[3];
    struct kwengine_selection_sample sample;
};
static inline int kwengine_selection_at(kwengine_selection_read_fn read, void *ctx, uint32_t base, uint32_t offset, void *out, size_t size)
{
    if (base == 0x400000U && offset >= 0x1000U) {
        base = (uint32_t)kwengine_va(0x400000U);
        offset = kwengine_rva(offset);
        if (!base || !offset)
            return 0;
    }
    return read && base >= 0x10000 && size <= 4096 && (uint64_t)base + offset + size <= UINT64_C(0x100000000) && read(ctx, base + offset, out, size);
}
static inline int kwengine_selection_once(kwengine_selection_read_fn read, void *ctx, struct kwengine_selection_internal *s)
{
    uint32_t v = 0, count = 0, indexed = 0;
    uint32_t player_list[2], squad_header[4];
    unsigned char frame_active[5], squad_defeated[5];
    memset(s, 0, sizeof(*s));
#define KWS_READ(b, o, x) kwengine_selection_at(read, ctx, (b), (o), &(x), sizeof(x))
    /* Coalesce only adjacent fields already required by this same snapshot.
     * Five-byte spans deliberately exclude padding/page-boundary overreads.
     * No data or permission is cached across calls or native callbacks. */
    if (!KWS_READ(0x400000, 0x7e1008, s->logic) || !KWS_READ(s->logic, 0, v) || v != kwengine_va(0xa99244) ||
        !KWS_READ(s->logic, 0x38, frame_active) || frame_active[4] != 1)
        return 0;
    memcpy(&s->sample.frame, frame_active, 4);
    if (!s->sample.frame || !KWS_READ(0x400000, 0x7e0b80, s->list) || !KWS_READ(s->list, 0, v) || v != kwengine_va(0xa21e44) ||
        !KWS_READ(s->list, 0x1c, player_list) || !(count = player_list[1]) || count > 26 || !(s->local = player_list[0]) ||
        !KWS_READ(s->local, 0, v) || v != kwengine_va(0xa237cc) ||
        !KWS_READ(s->local, 0x48, s->sample.player) || s->sample.player >= count || !KWS_READ(s->list, 0x24 + 4 * s->sample.player, indexed) ||
        indexed != s->local || !KWS_READ(s->local, 0x234, squad_defeated) || squad_defeated[4] != 0)
        return 0;
    memcpy(&s->squad, squad_defeated, 4);
    if (!s->squad)
        return 1; /* no Squad allocated yet */
    if (!KWS_READ(s->squad, 0, squad_header) || squad_header[0] != kwengine_va(0xa2233c))
        return 0;
    memcpy(s->vector, squad_header + 1, sizeof(s->vector));
    if (!s->vector[0] && !s->vector[1] && !s->vector[2])
        return 1;
    if (s->vector[0] < 0x10000 || (s->vector[0] & 3) || s->vector[1] < s->vector[0] || s->vector[2] < s->vector[1] || (s->vector[1] & 3) ||
        (s->vector[2] & 3))
        return 0;
    s->sample.count = (s->vector[1] - s->vector[0]) / 4;
    if (s->sample.count > KWENGINE_SELECTION_LIMIT)
        return 0;
    if (s->sample.count && !kwengine_selection_at(read, ctx, s->vector[0], 0, s->sample.ids, s->sample.count * 4))
        return 0;
    for (uint32_t i = 0; i < s->sample.count; ++i) {
        if (!s->sample.ids[i])
            return 0;
        for (uint32_t j = 0; j < i; ++j)
            if (s->sample.ids[i] == s->sample.ids[j])
                return 0;
    }
#undef KWS_READ
    return 1;
}
static inline int kwengine_selection_read(kwengine_selection_read_fn read, void *ctx, uint32_t image, struct kwengine_selection_sample *out)
{
    struct kwengine_selection_internal a, b;
    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (image != kwengine_va(0x400000U) || !kwengine_selection_once(read, ctx, &a) || !kwengine_selection_once(read, ctx, &b) ||
        memcmp(&a, &b, sizeof(a)))
        return 0;
    *out = a.sample;
    return 1;
}
#endif
