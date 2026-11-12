/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static unsigned fail_allocate, allocations;
static void *controlled_realloc(void *p, size_t n)
{
    ++allocations;
    return fail_allocate ? NULL : realloc(p, n);
}
#define realloc controlled_realloc
#include "../src/core/core.c"
#undef realloc
int main(void)
{
    struct kwf_registry r = {0};
    assert(kwf_reserve(NULL, 1, 0x1000, 4) == KWF_ERROR);
    assert(!kwf_patch_find(NULL, 1, 0x1000, 4));
    fail_allocate = 1;
    assert(kwf_reserve(&r, 1, 0x1000, 4) == KWF_ERROR && !r.count && !r.capacity && !r.patches);
    fail_allocate = 0;
    for (unsigned i = 0; i < 4097; ++i) {
        uint32_t owner = 1 + i % 3;
        uintptr_t address = 0x1000 + (uintptr_t)i * 16;
        assert(kwf_reserve(&r, owner, address, 5) == KWF_OK);
        struct kwf_patch *p = kwf_patch_find(&r, owner, address, 5);
        assert(p);
        p->original[0] = (unsigned char)i;
        p->replacement[0] = (unsigned char)(i ^ 0x55);
        p->written = i & 1;
        if (r.count == r.capacity) {
            struct kwf_patch *old = r.patches;
            uint32_t capacity = r.capacity, count = r.count;
            fail_allocate = 1;
            assert(kwf_reserve(&r, owner, address + 16, 5) == KWF_ERROR);
            assert(r.patches == old && r.capacity == capacity && r.count == count);
            unsigned attempts = allocations;
            assert(kwf_reserve(&r, owner, address, 5) == KWF_OK && allocations == attempts);
            fail_allocate = 0;
        }
    }
    assert(r.count == 4097 && r.capacity >= r.count);
    for (unsigned i = 0; i < r.count; ++i) {
        struct kwf_patch *p = kwf_patch_find(&r, 1 + i % 3, 0x1000 + (uintptr_t)i * 16, 5);
        assert(p);
        assert(p->original[0] == (unsigned char)i && p->replacement[0] == (unsigned char)(i ^ 0x55) && p->written == (i & 1));
    }
    unsigned count = r.count, attempts = allocations;
    assert(kwf_reserve(&r, 7, 0x1002, 3) == KWF_CONFLICT);
    assert(kwf_reserve(&r, 7, UINTPTR_MAX - 2, 5) == KWF_ERROR);
    assert(kwf_reserve(&r, 7, 0x10, KWF_MAX_PATCH_BYTES + 1) == KWF_ERROR);
    assert(r.count == count && allocations == attempts);
    kwf_registry_dispose(&r);
    assert(!r.count && !r.capacity && !r.patches);
    kwf_registry_dispose(&r);
    assert(kwf_reserve(&r, 9, 0x1000, 4) == KWF_OK);
    kwf_registry_dispose(&r);
    puts(
        "PASS patch registry: 4097 reservations, exact lookup/ownership/bytes across growth, allocation-failure atomicity, idempotence/conflicts, bounds and disposal");
    return 0;
}
