/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_PATH_QUEUE_DIAGNOSTIC_H
#define KWENGINE_PATH_QUEUE_DIAGNOSTIC_H
/* Internal read-only research layout. This discovers instructions, never patches
 * them. No engine hash allowlist or fallback to reference VAs is used. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define KWENGINE_PATH_QUEUE_SLOTS 512U
struct kwengine_path_section {
    uint32_t rva, size, flags;
};
struct kwengine_path_image {
    const unsigned char *bytes;
    uint32_t base, size, count;
    struct kwengine_path_section sections[96];
};
struct kwengine_path_layout {
    uint32_t logic_global, ai_global, player_global, data_global;
    uint32_t enqueue[3], queue_offset[3], contains, space, push;
    uint32_t retry_site, retry_return_site, request_site, request_return_site, waiting_offset, retry_offset;
    uint32_t logic_vtable, path_vtable, supply_vtable, list_vtable, player_vtable, squad_vtable;
    /* Optional v3 read-only nested harvest telemetry. Zero means unavailable. */
    uint32_t harvest_state_vtable, harvest_machine_vtable;
};
static inline uint16_t kwengine_path_u16(const void *v)
{
    const unsigned char *p = (const unsigned char *)v;
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t kwengine_path_u32(const void *v)
{
    const unsigned char *p = (const unsigned char *)v;
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int kwengine_path_range(uint32_t size, uint32_t at, size_t n)
{
    return at <= size && n <= size - at;
}
static inline int kwengine_path_span(const struct kwengine_path_image *p, uint32_t va, size_t n, uint32_t flags)
{
    if (va < p->base)
        return 0;
    uint32_t r = va - p->base;
    for (uint32_t i = 0; i < p->count; ++i) {
        const struct kwengine_path_section *s = &p->sections[i];
        if ((s->flags & flags) == flags && r >= s->rva && r - s->rva <= s->size && n <= s->size - (r - s->rva))
            return 1;
    }
    return 0;
}
static inline int kwengine_path_image_init(struct kwengine_path_image *p, const void *data, size_t available, uint32_t base)
{
    const unsigned char *b = (const unsigned char *)data;
    if (!p)
        return 0;
    memset(p, 0, sizeof(*p));
    if (!b || available < 64 || available > 256U * 1024U * 1024U || kwengine_path_u16(b) != 0x5a4d)
        return 0;
    uint32_t nt = kwengine_path_u32(b + 60);
    if (!kwengine_path_range((uint32_t)(available > UINT32_MAX ? UINT32_MAX : available), nt, 24))
        return 0;
    uint32_t optional = kwengine_path_u16(b + nt + 20), count = kwengine_path_u16(b + nt + 6), table = nt + 24 + optional;
    if (kwengine_path_u32(b + nt) != 0x4550 || kwengine_path_u16(b + nt + 4) != 0x14c || optional < 96 || !count || count > 96 ||
        (uint64_t)table + count * 40U > available || kwengine_path_u16(b + nt + 24) != 0x10b)
        return 0;
    uint32_t size = kwengine_path_u32(b + nt + 80);
    if (size < 4096 || size > available || (uint64_t)base + size > UINT64_C(0x100000000))
        return 0;
    p->bytes = b;
    p->base = base;
    p->size = size;
    p->count = count;
    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char *s = b + table + i * 40;
        struct kwengine_path_section *o = &p->sections[i];
        o->rva = kwengine_path_u32(s + 12);
        o->size = kwengine_path_u32(s + 8);
        o->flags = kwengine_path_u32(s + 36);
        if (!kwengine_path_range(size, o->rva, o->size))
            return 0;
    }
    return 1;
}
/* Scan readable executable sections; zero mask bytes are relocation operands. */
static inline unsigned kwengine_path_find(const struct kwengine_path_image *p, const unsigned char *b, const unsigned char *mask, size_t n,
                                          uint32_t *hits, unsigned capacity)
{
    unsigned count = 0;
    for (uint32_t s = 0; s < p->count; ++s) {
        const struct kwengine_path_section *sec = &p->sections[s];
        if ((sec->flags & 0x60000000U) != 0x60000000U || n > sec->size)
            continue;
        for (uint32_t i = 0; i <= sec->size - n; ++i) {
            const unsigned char *q = p->bytes + sec->rva + i;
            if (q[0] != b[0])
                continue;
            size_t j;
            for (j = 1; j < n; ++j)
                if (mask[j] && q[j] != b[j])
                    break;
            if (j == n) {
                if (count < capacity)
                    hits[count] = p->base + sec->rva + i;
                ++count;
                if (count > capacity)
                    return count;
            }
        }
    }
    return count;
}
static inline uint32_t kwengine_path_rtti(const struct kwengine_path_image *p, const char *name)
{
    uint32_t descriptor = 0, locator = 0, table = 0;
    unsigned dn = 0, ln = 0, tn = 0;
    size_t length = strlen(name) + 1;
    for (uint32_t k = 0; k < p->count; ++k) {
        const struct kwengine_path_section *s = &p->sections[k];
        if (!(s->flags & 0x40000000U) || s->flags & 0x20000000U || length > s->size)
            continue;
        for (uint32_t j = 8; j <= s->size - length; ++j)
            if (p->bytes[s->rva + j] == '.' && !memcmp(p->bytes + s->rva + j, name, length)) {
                descriptor = p->base + s->rva + j - 8;
                ++dn;
            }
    }
    if (dn != 1)
        return 0;
    for (uint32_t k = 0; k < p->count; ++k) {
        const struct kwengine_path_section *s = &p->sections[k];
        if (!(s->flags & 0x40000000U) || s->flags & 0xa0000000U || s->size < 20)
            continue;
        for (uint32_t j = 0; j <= s->size - 20; j += 4) {
            const unsigned char *q = p->bytes + s->rva + j;
            if (!kwengine_path_u32(q) && !kwengine_path_u32(q + 4) && !kwengine_path_u32(q + 8) && kwengine_path_u32(q + 12) == descriptor &&
                kwengine_path_span(p, kwengine_path_u32(q + 16), 16, 0x40000000U)) {
                locator = p->base + s->rva + j;
                ++ln;
            }
        }
    }
    if (ln != 1)
        return 0;
    for (uint32_t k = 0; k < p->count; ++k) {
        const struct kwengine_path_section *s = &p->sections[k];
        if (!(s->flags & 0x40000000U) || s->flags & 0xa0000000U || s->size < 8)
            continue;
        for (uint32_t j = 0; j <= s->size - 8; j += 4) {
            const unsigned char *q = p->bytes + s->rva + j;
            if (kwengine_path_u32(q) == locator && kwengine_path_span(p, kwengine_path_u32(q + 4), 1, 0x20000000U)) {
                table = p->base + s->rva + j + 4;
                ++tn;
            }
        }
    }
    return tn == 1 ? table : 0;
}
static inline uint32_t kwengine_path_call(const struct kwengine_path_image *p, uint32_t site)
{
    if (!kwengine_path_span(p, site, 5, 0x60000000U) || p->bytes[site - p->base] != 0xe8)
        return 0;
    uint32_t dest = site + 5 + kwengine_path_u32(p->bytes + site - p->base + 1);
    return kwengine_path_span(p, dest, 1, 0x60000000U) ? dest : 0;
}
static inline uint32_t kwengine_path_global(const struct kwengine_path_image *p, const unsigned char *b, const unsigned char *m, size_t n)
{
    uint32_t hit;
    if (kwengine_path_find(p, b, m, n, &hit, 1) != 1)
        return 0;
    uint32_t v = kwengine_path_u32(p->bytes + hit - p->base + 1);
    return kwengine_path_span(p, v, 4, 0x40000000U) ? v : 0;
}
static inline int kwengine_path_discover(const struct kwengine_path_image *p, struct kwengine_path_layout *out)
{
    static const unsigned char enqueue[] = {0x56, 0x8d, 0xb1, 0, 0,    0,    0,    0x8d, 0x44, 0x24, 8,    0x50, 0x8b,
                                            0xce, 0xe8, 0,    0, 0,    0,    0x84, 0xc0, 0x75, 0x17, 0x8b, 0xce, 0xe8,
                                            0,    0,    0,    0, 0x84, 0xc0, 0x74, 0x0e, 0x8d, 0x44, 0x24, 8,    0x50,
                                            0x8b, 0xce, 0xe8, 0, 0,    0,    0,    0xb0, 1,    0x5e, 0xc2, 4,    0};
    static const unsigned char enqueue_mask[] = {255, 255, 255, 0,   0,   0,   0,   255, 255, 255, 255, 255, 255, 255, 255, 0,   0,   0,
                                                 0,   255, 255, 255, 255, 255, 255, 255, 0,   0,   0,   0,   255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 0,   0,   0,   0,   255, 255, 255, 255, 255, 255};
    static const unsigned char retry[] = {0xa1, 0,    0,    0,    0,    0x8b, 0x48, 0x38, 0x8b, 0x83, 0x8c, 1, 0, 0,    0x85, 0xc0, 0x74, 0x2d,
                                          0x3b, 0xc8, 0x72, 0x1f, 0x8b, 0x43, 0x0c, 0xff, 0xb0, 0xec, 0,    0, 0, 0xa1, 0,    0,    0,    0,
                                          0x8b, 0x48, 0x1c, 0xe8, 0,    0,    0,    0,    0x83, 0xa3, 0x8c, 1, 0, 0,    0,    0xeb, 0x0a};
    static const unsigned char retry_mask[] = {255, 0,   0,   0,   0,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                               255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   0,   0,   0,
                                               255, 255, 255, 255, 0,   0,   0,   0,   255, 255, 255, 255, 255, 255, 255, 255, 255};
    /* Existing shared resolver anchors for PlayerList and GlobalData. */
    static const unsigned char player[] = {0xa1, 0,    0,    0,    0,    0x85, 0xc0, 0x6a, 2,    0x5f, 0x74, 0, 0x8b,
                                           0x40, 0x1c, 0x85, 0xc0, 0x74, 0,    0x39, 0xb8, 0x4c, 2,    0,    0};
    static const unsigned char player_mask[] = {255, 0,   0,   0,   0,   255, 255, 255, 255, 255, 255, 0,  255,
                                                255, 255, 255, 255, 255, 0,   255, 255, 255, 255, 255, 255};
    static const unsigned char data[] = {0xa1, 0, 0, 0, 0, 0x80, 0xb8, 0x7c, 0x0d, 0, 0, 0, 0x89, 0x3d, 0, 0, 0, 0, 0x74, 0, 0x8b, 0x0d, 0, 0, 0, 0};
    static const unsigned char data_mask[] = {255, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 255, 0, 255, 255, 0, 0, 0, 0};
    struct kwengine_path_layout l;
    if (!out)
        return 0;
    memset(&l, 0, sizeof(l));
    memset(out, 0, sizeof(*out));
    if (sizeof(enqueue) != sizeof(enqueue_mask) || sizeof(retry) != sizeof(retry_mask))
        return 0;
    if (kwengine_path_find(p, enqueue, enqueue_mask, sizeof(enqueue), l.enqueue, 3) != 3 ||
        kwengine_path_find(p, retry, retry_mask, sizeof(retry), &l.retry_site, 1) != 1)
        return 0;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char *q = p->bytes + l.enqueue[i] - p->base;
        l.queue_offset[i] = kwengine_path_u32(q + 3);
        if (l.queue_offset[i] > 0x100000 || (l.queue_offset[i] & 3))
            return 0;
        uint32_t contains = kwengine_path_call(p, l.enqueue[i] + 14), space = kwengine_path_call(p, l.enqueue[i] + 25),
                 push = kwengine_path_call(p, l.enqueue[i] + 41);
        if (!contains || !space || !push || (i && (contains != l.contains || space != l.space || push != l.push)))
            return 0;
        l.contains = contains;
        l.space = space;
        l.push = push;
    }
    /* Discovery order is code order: require the three contiguous queue schemas. */
    if (l.queue_offset[1] != l.queue_offset[0] + 0x808 || l.queue_offset[2] != l.queue_offset[1] + 0x808)
        return 0;
    static const unsigned char space_code[] = {0x8b, 0x81, 4,    8,    0,    0,    0x8b, 0xd0, 0x81, 0xea, 0xff, 1, 0,    0,    0x40, 0xf7, 0xda,
                                               0x1b, 0xd2, 0x23, 0xd0, 0x33, 0xc0, 0x3b, 0x91, 0,    8,    0,    0, 0x0f, 0x95, 0xc0, 0xc3};
    if (!kwengine_path_span(p, l.space, sizeof(space_code), 0x60000000U) || memcmp(p->bytes + l.space - p->base, space_code, sizeof(space_code)))
        return 0;
    static const unsigned char contains_code[] = {0x8b, 0x81, 0x00, 0x08, 0x00, 0x00, 0x8b, 0x91, 0x04, 0x08, 0x00, 0x00, 0x3b, 0xc2, 0x56,
                                                  0x57, 0x74, 0x1e, 0x8b, 0x74, 0x24, 0x0c, 0x8b, 0x36, 0x39, 0x34, 0x81, 0x74, 0x1a, 0x8b,
                                                  0xf8, 0x81, 0xef, 0xff, 0x01, 0x00, 0x00, 0x40, 0xf7, 0xdf, 0x1b, 0xff, 0x23, 0xc7, 0x3b,
                                                  0xc2, 0x75, 0xe8, 0x32, 0xc0, 0x5f, 0x5e, 0xc2, 0x04, 0x00, 0xb0, 0x01, 0xeb, 0xf7};
    if (!kwengine_path_span(p, l.contains, sizeof(contains_code), 0x60000000U) ||
        memcmp(p->bytes + l.contains - p->base, contains_code, sizeof(contains_code)))
        return 0;
    static const unsigned char push_code[] = {0x8b, 0x81, 0x04, 0x08, 0x00, 0x00, 0x8b, 0x54, 0x24, 0x04, 0x8b, 0x12, 0x89, 0x14, 0x81,
                                              0x8b, 0x81, 0x04, 0x08, 0x00, 0x00, 0x8b, 0xd0, 0x81, 0xea, 0xff, 0x01, 0x00, 0x00, 0x40,
                                              0xf7, 0xda, 0x1b, 0xd2, 0x23, 0xd0, 0x89, 0x91, 0x04, 0x08, 0x00, 0x00, 0xc2, 0x04, 0x00};
    if (!kwengine_path_span(p, l.push, sizeof(push_code), 0x60000000U) || memcmp(p->bytes + l.push - p->base, push_code, sizeof(push_code)))
        return 0;
    const unsigned char *q = p->bytes + l.retry_site - p->base;
    l.logic_global = kwengine_path_u32(q + 1);
    l.ai_global = kwengine_path_u32(q + 32);
    l.retry_offset = kwengine_path_u32(q + 10);
    l.retry_return_site = l.retry_site + 51;
    l.waiting_offset = 0x3d9;
    if (kwengine_path_call(p, l.retry_site + 39) != l.enqueue[0] || !kwengine_path_span(p, l.logic_global, 4, 0x40000000U) ||
        !kwengine_path_span(p, l.ai_global, 4, 0x40000000U))
        return 0;
    static const unsigned char request[] = {0x8b, 0x43, 0x0c, 0xc6, 0x83, 0, 0,    0,    0,    1,    0xff, 0xb0, 0xec, 0, 0,
                                            0,    0xa1, 0,    0,    0,    0, 0x8b, 0x48, 0x1c, 0xe8, 0,    0,    0,    0};
    static const unsigned char request_mask[] = {255, 255, 255, 255, 255, 0, 0,   0,   0,   255, 255, 255, 255, 255, 255,
                                                 255, 255, 0,   0,   0,   0, 255, 255, 255, 255, 0,   0,   0,   0};
    uint32_t requests[8];
    unsigned request_count = kwengine_path_find(p, request, request_mask, sizeof(request), requests, 8);
    if (!request_count || request_count > 8)
        return 0;
    for (unsigned i = 0; i < request_count; ++i) {
        const unsigned char *r = p->bytes + requests[i] - p->base;
        uint32_t waiting = kwengine_path_u32(r + 5);
        if (waiting != 0x3d9 || kwengine_path_u32(r + 17) != l.ai_global || kwengine_path_call(p, requests[i] + 24) != l.enqueue[0])
            return 0;
        l.waiting_offset = waiting;
    }
    l.request_site = requests[0];
    l.request_return_site = l.request_site + sizeof(request);
    l.player_global = kwengine_path_global(p, player, player_mask, sizeof(player));
    l.data_global = kwengine_path_global(p, data, data_mask, sizeof(data));
    l.logic_vtable = kwengine_path_rtti(p, ".?AVW3DGameLogic@@");
    l.path_vtable = kwengine_path_rtti(p, ".?AVPathfinder@@");
    l.supply_vtable = kwengine_path_rtti(p, ".?AVSupplyTruckAIUpdate@@");
    l.list_vtable = kwengine_path_rtti(p, ".?AVPlayerList@@");
    l.player_vtable = kwengine_path_rtti(p, ".?AVPlayer@@");
    l.squad_vtable = kwengine_path_rtti(p, ".?AVSquad@@");
    if (!l.player_global || !l.data_global || !l.logic_vtable || !l.path_vtable || !l.supply_vtable || !l.list_vtable || !l.player_vtable ||
        !l.squad_vtable)
        return 0;
    /* Corroborate the native AIUpdate module's +40 state machine (not +44), State+4 ID,
     * and supply module +514 pointer via its +14 UpdateModule receiver. */
    static const unsigned char state_getter[] = {0x8b, 0x49, 0x40, 0x85, 0xc9, 0x75, 0x04, 0x6a, 0x55, 0x58, 0xc3, 0x8b, 0x41, 0x54, 0x85, 0xc0,
                                                 0xba, 0x3f, 0x42, 0x0f, 0x00, 0x74, 0x05, 0x8b, 0x40, 0x04, 0xeb, 0x02, 0x8b, 0xc2, 0x3b, 0xc2,
                                                 0x75, 0x0d, 0x8b, 0x49, 0x04, 0x85, 0xc9, 0x74, 0x04, 0x8b, 0x41, 0x04, 0xc3, 0x8b, 0xc2, 0xc3};
    static const unsigned char supply_update[] = {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0xf8, 0x8b, 0x80, 0x84, 0x00, 0x00, 0x00,
                                                  0xc1, 0xe8, 0x09, 0xa8, 0x01, 0x75, 0x26, 0x8b, 0x8e, 0x00, 0x05, 0x00,
                                                  0x00, 0x8b, 0x01, 0x57, 0xff, 0x50, 0x10, 0x85, 0xc0, 0x7e};
    unsigned char exact[sizeof(state_getter)];
    memset(exact, 255, sizeof(exact));
    uint32_t hit;
    if (kwengine_path_find(p, state_getter, exact, sizeof(state_getter), &hit, 1) != 1 ||
        kwengine_path_find(p, supply_update, exact, sizeof(supply_update), &hit, 1) != 1)
        return 0;
    /* AIHarvestState::update reads its owned inner machine at +20 and calls
     * its update slot. Match the actual RTTI-bound body, not a reference VA.
     * Absence disables only these optional fields, retaining the v2 capture. */
    uint32_t harvest_state = kwengine_path_rtti(p, ".?AVAIHarvestState@@");
    uint32_t harvest_machine = kwengine_path_rtti(p, ".?AVAIHarvestMachine@@");
    static const unsigned char harvest_update[] = {0x8b, 0x49, 0x20, 0x8b, 0x01, 0xff, 0x50, 0x10, 0x85, 0xc0, 0x7e, 0x02, 0x33, 0xc0, 0xc3};
    if (harvest_state && harvest_machine && kwengine_path_span(p, harvest_state, 28, 0x40000000U)) {
        uint32_t update = kwengine_path_u32(p->bytes + harvest_state - p->base + 24);
        if (kwengine_path_span(p, update, sizeof(harvest_update), 0x60000000U) &&
            !memcmp(p->bytes + update - p->base, harvest_update, sizeof(harvest_update))) {
            l.harvest_state_vtable = harvest_state;
            l.harvest_machine_vtable = harvest_machine;
        }
    }
    *out = l;
    return 1;
}
/* Stable queue header required before interpreting entries; one slot is unused. */
static inline int kwengine_path_queue_count(uint32_t head, uint32_t tail, uint32_t *count)
{
    if (head >= KWENGINE_PATH_QUEUE_SLOTS || tail >= KWENGINE_PATH_QUEUE_SLOTS)
        return 0;
    *count = (tail + KWENGINE_PATH_QUEUE_SLOTS - head) % KWENGINE_PATH_QUEUE_SLOTS;
    return 1;
}
#endif
