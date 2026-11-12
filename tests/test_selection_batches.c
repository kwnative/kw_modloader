/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
/* SDK-only reader regression; no plugin, engine process or game image needed. */
#include <kwengine/selection_reader.h>
#include <assert.h>
#include <stdio.h>

static unsigned calls, fail_at, change_at;
static uint32_t logic = 0x10000, list = 0x20000, local = 0x30000, squad = 0x40000, ids = 0x50000;
static unsigned char logic_data[0x3d], list_data[0x8c], local_data[0x239], squad_data[16], id_data[512];
static void word(void *raw, size_t offset, uint32_t value) { memcpy((unsigned char *)raw + offset, &value, 4); }
static int read_region(uint32_t address, void *out, size_t size, uint32_t base, const void *data, size_t bytes)
{
    if (address < base || size > bytes || address - base > bytes - size)
        return 0;
    memcpy(out, (const unsigned char *)data + (address - base), size);
    return 1;
}
static int read_data(void *ctx, uint32_t address, void *out, size_t size)
{
    (void)ctx;
    if (++calls == fail_at)
        return 0;
    if (calls == change_at)
        word(logic_data, 0x38, 32);
    if (size == 4 && address == 0xbe1008) { memcpy(out, &logic, 4); return 1; }
    if (size == 4 && address == 0xbe0b80) { memcpy(out, &list, 4); return 1; }
    return read_region(address, out, size, logic, logic_data, sizeof(logic_data)) ||
           read_region(address, out, size, list, list_data, sizeof(list_data)) ||
           read_region(address, out, size, local, local_data, sizeof(local_data)) ||
           read_region(address, out, size, squad, squad_data, sizeof(squad_data)) ||
           read_region(address, out, size, ids, id_data, sizeof(id_data));
}
static void reset(void)
{
    memset(logic_data, 0, sizeof(logic_data));
    memset(list_data, 0, sizeof(list_data));
    memset(local_data, 0, sizeof(local_data));
    memset(squad_data, 0, sizeof(squad_data));
    memset(id_data, 0, sizeof(id_data));
    word(logic_data, 0, 0xa99244);
    word(logic_data, 0x38, 31);
    logic_data[0x3c] = 1;
    word(list_data, 0, 0xa21e44);
    word(list_data, 0x1c, local);
    word(list_data, 0x20, 2);
    word(list_data, 0x28, local);
    word(local_data, 0, 0xa237cc);
    word(local_data, 0x48, 1);
    word(local_data, 0x234, squad);
    word(squad_data, 0, 0xa2233c);
    word(squad_data, 4, ids);
    word(squad_data, 8, ids + 8);
    word(squad_data, 12, ids + 8);
    word(id_data, 0, 11);
    word(id_data, 4, 12);
    calls = fail_at = change_at = 0;
}
int main(void)
{
    struct kwengine_selection_internal value;
    struct kwengine_selection_sample sample;
    reset();
    assert(kwengine_selection_once(read_data, NULL, &value) && calls == 12);
    assert(value.logic == logic && value.list == list && value.local == local && value.squad == squad &&
           value.sample.frame == 31 && value.sample.player == 1 && value.sample.count == 2 &&
           value.sample.ids[0] == 11 && value.sample.ids[1] == 12);
    for (unsigned fault = 1; fault <= 12; ++fault) {
        reset(); fail_at = fault;
        assert(!kwengine_selection_once(read_data, NULL, &value));
    }
    reset();
    assert(kwengine_selection_read(read_data, NULL, 0x400000, &sample) && calls == 24);
    reset(); change_at = 15;
    assert(!kwengine_selection_read(read_data, NULL, 0x400000, &sample)); // Independent closing snapshot remains mandatory.
    reset(); local_data[0x238] = 1;
    assert(!kwengine_selection_once(read_data, NULL, &value));
    reset(); logic_data[0x3c] = 0;
    assert(!kwengine_selection_once(read_data, NULL, &value));
    reset(); word(list_data, 0x28, 0x123456);
    assert(!kwengine_selection_once(read_data, NULL, &value));
    reset(); word(id_data, 4, 11);
    assert(!kwengine_selection_once(read_data, NULL, &value));
    reset(); word(squad_data, 8, ids + 129 * 4); word(squad_data, 12, ids + 129 * 4);
    assert(!kwengine_selection_once(read_data, NULL, &value));
    reset(); word(local_data, 0x234, 0);
    assert(kwengine_selection_once(read_data, NULL, &value) && !value.squad && !value.sample.count && calls == 10);
    puts("PASS coalesced selection: 12 reads, exact five-byte ends, every fault refused, fresh closing pass, defeat/identity/group fences");
    return 0;
}
