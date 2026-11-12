/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
/* Read-only mapped-image fixture. Private input images are command arguments;
 * no game executable, constructor, import, callback or native code is run.
 * Mutations below affect only an anonymous, non-executable mapped copy.
 */
#define _GNU_SOURCE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include "../src/engine/engine_address.c"
#include <kwengine/map_sources.h>
#include <kwengine/replay_view_sources.h>
#include <kwengine/beacon_sources.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static const struct engine_symbol *symbol(uint32_t reference)
{
    for (size_t i = 0; i < ENGINE_COUNT; ++i)
        if (engine_symbols[i].reference == reference)
            return &engine_symbols[i];
    abort();
}
static void lobby_ui_bindings(void)
{
    const uint32_t references[] = {0x40d028U, 0x4116faU, 0x4153d1U, 0x41610eU, 0x41df2bU, 0x422ee9U, 0x42c24fU, 0x484770U};
    for (unsigned i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
        const struct engine_symbol *s = symbol(references[i]);
        unsigned char *entry = (void *)kwengine_va(references[i]);
        assert(entry && kwengine_patch_site_valid((uintptr_t)entry, s->prefix_length));
        entry[0] ^= 1;
        assert(!kwengine_patch_site_valid((uintptr_t)entry, s->prefix_length));
        entry[0] ^= 1;
        assert(kwengine_patch_site_valid((uintptr_t)entry, s->prefix_length));
    }
    /* Complete stolen instructions: PUSH EBP; MOV EBP,ESP; MOV EAX,[global].
     * The absolute operand is already relocated in the loaded process. */
    const unsigned char *row = (void *)kwengine_va(0x422ee9U);
    assert(row[0] == 0x55 && row[1] == 0x8b && row[2] == 0xec && row[3] == 0xa1);
    assert(kwengine_is_data(engine_u32(row + 4), 4));
    const uint32_t calls[][2] = {{0x44f291U, 0x416127U}, {0x44f26bU, 0x416171U}};
    for (unsigned i = 0; i < 2; ++i) {
        unsigned char *call = (void *)kwengine_va(calls[i][0]);
        const struct engine_symbol *s = symbol(calls[i][0]);
        assert(call[0] == 0xe8 && kwengine_patch_site_valid((uintptr_t)call, s->prefix_length));
        assert((uintptr_t)call + 5 + (int32_t)engine_u32(call + 1) == kwengine_va(calls[i][1]));
        call[1] ^= 1;
        assert((uintptr_t)call + 5 + (int32_t)engine_u32(call + 1) != kwengine_va(calls[i][1]));
        call[1] ^= 1;
        call[0] ^= 1;
        assert(!kwengine_patch_site_valid((uintptr_t)call, s->prefix_length));
        call[0] ^= 1;
    }
    assert(kwengine_patch_site_valid(kwengine_va(0x41df8dU), 22));
    assert(kwengine_is_data(kwengine_va(0xa9e004U), 4));
    assert(kwengine_is_data(kwengine_va(0xa9eabcU), 4));
    assert(kwengine_va(0xa9e004U) != kwengine_va(0xa9eabcU));
    assert(kwengine_is_data(kwengine_va(0xbdd63cU), 4));
    assert(kwengine_is_data(kwengine_va(0xa204dcU), 0x44));
    assert(engine_u32((void *)(kwengine_va(0xa204dcU) + 0x40)) == kwengine_va(0x484770U));
    printf("resolved palette-selection=%08lx palette-metadata=%08lx\n", (unsigned long)kwengine_va(0x416127U),
           (unsigned long)kwengine_va(0x416171U));
    puts("PASS distinct palette/combo RTTI, palette count and fresh selection/metadata call targets; no native calls");
    puts("PASS lobby clear/enable/row-hook discovery, fresh prefix refusal and complete stolen-instruction shape; no native calls");
}
static void beacon_bindings(void)
{
    struct kwengine_beacon_sources b, zero = {0};
    assert(kwengine_beacon_sources_read(&b));
    assert(b.notification == kwengine_va(0x74d829U) && b.caller == kwengine_va(0x51b684U) + 5);
    unsigned char *call = (void *)(b.caller - 5), *body = (void *)b.notification;
    call[1] ^= 1;
    assert(!kwengine_beacon_sources_read(&b) && !memcmp(&b, &zero, sizeof(b)));
    call[1] ^= 1;
    body[0] ^= 1;
    assert(!kwengine_beacon_sources_read(&b) && !memcmp(&b, &zero, sizeof(b)));
    body[0] ^= 1;
    body[47] ^= 1;
    assert(!kwengine_beacon_sources_read(&b) && !memcmp(&b, &zero, sizeof(b)));
    body[47] ^= 1;
    assert(kwengine_beacon_sources_read(&b));
    puts("PASS beacon call target, complete entry and cdecl-return authority");
}
static size_t symbol_index(uint32_t reference)
{
    return (size_t)(symbol(reference) - engine_symbols);
}
static void ready(unsigned char *image, uint32_t size)
{
    struct kwengine_resolution status;
    const int result = kwengine_resolve(image, size, &status);
    if (!result)
        fprintf(stderr, "resolver %u/%u missing=%08x\n", status.resolved, status.expected, status.missing_reference);
    assert(result && kwengine_is_ready());
}
static void denied(unsigned char *image, uint32_t size)
{
    struct kwengine_resolution status;
    struct kwengine_map_source_bindings actual, zero = {0};
    struct kwengine_object_fog_bindings fog, fog_zero = {0};
    struct kwengine_initial_map_hook_bindings initial, initial_zero = {0};
    struct kwengine_replay_view_bindings replay, replay_zero = {0};
    assert(!kwengine_resolve(image, size, &status) && !kwengine_is_ready());
    memset(&actual, 0xff, sizeof(actual));
    assert(!kwengine_map_sources(&actual) && !memcmp(&actual, &zero, sizeof(actual)));
    memset(&fog, 0xff, sizeof(fog));
    assert(!kwengine_object_fog_sources(&fog) && !memcmp(&fog, &fog_zero, sizeof(fog)));
    memset(&initial, 0xff, sizeof(initial));
    assert(!kwengine_initial_map_hook_sources(&initial) && !memcmp(&initial, &initial_zero, sizeof(initial)));
    memset(&replay, 0xff, sizeof(replay));
    assert(!kwengine_replay_view_sources(&replay) && !memcmp(&replay, &replay_zero, sizeof(replay)));
    assert(!kwengine_va(0xb70660U) && !kwengine_va(0xbe7eecU));
}
static unsigned char *data_space(unsigned char *image)
{
    for (uint32_t i = 0; i < engine.section_count; ++i) {
        const struct engine_section *section = &engine.sections[i];
        if ((section->flags & 0xe0000000U) != 0xc0000000U)
            continue;
        for (uint32_t at = 0; at + 8 <= section->size; at += 4) {
            unsigned char *p = image + section->start + at;
            if (!engine_u32(p) && !engine_u32(p + 4))
                return p;
        }
    }
    abort();
}
static void named_bindings(unsigned char *image, uint32_t size)
{
    (void)size;
    struct kwengine_map_source_bindings bindings;
    assert(kwengine_map_sources(&bindings));
    for (uint32_t i = 0; i < KWENGINE_RESOURCE_PROPERTY_COUNT; ++i) {
        const struct engine_symbol *source = NULL;
        assert(kwengine_map_sources(&bindings));
        const uint32_t wrapper = bindings.resource_keys[i];
        for (size_t n = 0; n < ENGINE_COUNT; ++n)
            if (engine.addresses[n] == wrapper && engine_symbols[n].kind == ENGINE_NAMED_DATA) {
                assert(!source);
                source = &engine_symbols[n];
            }
        assert(source);
        const size_t index = symbol_index(source->reference);
        const uint32_t text = engine_u32((void *)(uintptr_t)(wrapper + 4));
        unsigned char *slot = data_space(image);
        assert(slot != (void *)(uintptr_t)wrapper);
        const uint32_t original_value = engine_u32((void *)(uintptr_t)wrapper);
        /* Runtime lazy NameKey changes are not asset hashes/signature bytes. */
        const uint32_t changed_key = original_value ^ 0x5a5a5a5aU;
        memcpy((void *)(uintptr_t)wrapper, &changed_key, 4);
        assert(engine_named_data(source) == wrapper);
        memcpy((void *)(uintptr_t)wrapper, &original_value, 4);

        memcpy(slot + 4, &text, 4);
        assert(!engine_named_data(source)); /* No "first wins" on duplicates. */
        memset((void *)(uintptr_t)(wrapper + 4), 0, 4);
        engine.addresses[index] = engine_named_data(source);
        assert(kwengine_map_sources(&bindings) && bindings.resource_keys[i] == (uintptr_t)slot);
        memcpy((void *)(uintptr_t)(wrapper + 4), &text, 4);
        memset(slot, 0, 8);
        engine.addresses[index] = engine_named_data(source);
        assert(engine.addresses[index] == wrapper);

        unsigned char *literal = (void *)(uintptr_t)text;
        literal[0] ^= 0x20;
        assert(!engine_named_data(source));
        literal[0] ^= 0x20;
        assert(engine_named_data(source) == wrapper);
    }
    puts("PASS focused named-data relocation/duplicates/missing and mutable-key checks");
}
static void global_bindings(unsigned char *image, uint32_t size)
{
    (void)size;
    const uint32_t references[] = {0xb70660U, 0xbe7eecU};
    for (size_t i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
        const struct engine_symbol *source = symbol(references[i]);
        const uint32_t at = engine_unique_pattern(source);
        assert(at && source->kind == ENGINE_PATTERN_ABSOLUTE);
        unsigned char *operand = image + at + source->offset;
        const uint32_t original = engine_u32(operand);
        const uint32_t relocated = original + 4;
        memcpy(operand, &relocated, 4);
        assert(engine_u32(image + engine_unique_pattern(source) + source->offset) == relocated);
        memcpy(operand, &original, 4);
        image[at] ^= 0x80;
        assert(!engine_unique_pattern(source));
        image[at] ^= 0x80;
        assert(engine_unique_pattern(source) == at);

        /* A mapped operand can still have the wrong section/authority. */
        const uint32_t code = engine.base + at;
        memcpy(operand, &code, 4);
        engine.addresses[symbol_index(references[i])] = code;
        struct kwengine_map_source_bindings actual, zero = {0};
        assert(!kwengine_map_sources(&actual) && !memcmp(&actual, &zero, sizeof(actual)));
        memcpy(operand, &original, 4);
        engine.addresses[symbol_index(references[i])] = original;
    }
    puts("PASS focused global operands/missing witnesses and typed data authority");
}
static void rtti_bindings(unsigned char *image, uint32_t size)
{
    (void)image;
    (void)size;
    const uint32_t references[] = {0xa50028U, 0xa88dacU, 0xa9a8acU, 0xa9a99cU};
    for (size_t i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
        const uint32_t vtable = (uint32_t)kwengine_va(references[i]);
        const uint32_t locator = engine_u32((void *)(uintptr_t)(vtable - 4));
        const uint32_t descriptor = engine_u32((void *)(uintptr_t)(locator + 12));
        unsigned char *name = (void *)(uintptr_t)(descriptor + 8);
        assert(kwengine_is_code(engine_u32((void *)(uintptr_t)vtable)));
        name[0] ^= 0x20;
        assert(!engine_rtti(symbol(references[i])));
        name[0] ^= 0x20;
        assert(engine_rtti(symbol(references[i])) == vtable);
    }
    puts("PASS focused native primary/secondary RTTI refusal/restoration");
}
static void object_fog_bindings(void)
{
    struct kwengine_object_fog_bindings sources, zero = {0};
    assert(!kwengine_object_fog_sources(NULL));
    assert(kwengine_object_fog_sources(&sources));
    const uint32_t secondary = sources.shroud_interface_vtable;
    const uint32_t primary = (uint32_t)kwengine_va(0xa88d88U);
    assert(secondary != primary);
    const uint32_t locator = engine_u32((void *)(uintptr_t)(secondary - 4));
    const uint32_t original_offset = engine_u32((void *)(uintptr_t)(locator + 4));
    assert(original_offset == 0x64);
    /* A same-class locator with the wrong secondary-this displacement is not
     * an equivalent RTTI binding. No copied layout fallback. */
    memset((void *)(uintptr_t)(locator + 4), 0, 4);
    assert(!engine_rtti(symbol(0xa88dacU)));
    memcpy((void *)(uintptr_t)(locator + 4), &original_offset, 4);
    assert(engine_rtti(symbol(0xa88dacU)) == secondary);

    unsigned char *slot = (void *)(uintptr_t)(secondary + 0x14);
    const uint32_t target = engine_u32(slot);
    assert(target == sources.hidden_when_fogged);
    const uint32_t wrong_method = engine_u32((void *)(uintptr_t)(primary + 0x14));
    assert(wrong_method != target && kwengine_is_code(wrong_method));
    memcpy(slot, &wrong_method, 4);
    assert(!kwengine_object_fog_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
    memcpy(slot, &target, 4);
    assert(kwengine_object_fog_sources(&sources));

    unsigned char *entry = (void *)(uintptr_t)target;
    /* The entire short prefix is authority, not just its first opcode. */
    for (uint32_t i = 0; i < 33; ++i) {
        entry[i] ^= 0x80;
        assert(!kwengine_object_fog_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
        entry[i] ^= 0x80;
    }
    const size_t table_index = symbol_index(0xa88dacU);
    engine.addresses[table_index] = target; /* Executable span is not vtable data. */
    assert(!kwengine_object_fog_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
    engine.addresses[table_index] = secondary;
    assert(kwengine_object_fog_sources(&sources));
    printf("resolved shroud-interface=%08x hide-in-fog=%08x secondary-this=%x\n", secondary, target, original_offset);
    puts("PASS secondary Object shroud RTTI, this-offset, slot target, full prefix and data authority; no native calls");
}
static void hook_bindings(void)
{
    const uint32_t references[] = {0x782476U, 0x7a462cU, 0x7823b6U, 0x782370U};
    struct kwengine_ghost_hook_bindings sources, zero = {0};
    assert(kwengine_ghost_hook_sources(&sources));
    for (size_t i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
        const struct engine_symbol *source = symbol(references[i]);
        unsigned char *entry = (void *)kwengine_va(references[i]);
        assert(source->prefix_length && source->prefix_mask[0] == 0xff);
        entry[0] ^= 0x80;
        assert(!kwengine_ghost_hook_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
        entry[0] ^= 0x80;
        assert(kwengine_ghost_hook_sources(&sources));
    }
    puts("PASS four ghost lifecycle hook-site prefixes; changed live code refused without native calls/patches");
}
static void initial_map_bindings(void)
{
    struct kwengine_initial_map_hook_bindings sources, zero = {0};
    assert(!kwengine_initial_map_hook_sources(NULL) && kwengine_initial_map_hook_sources(&sources));
    const uint32_t entries[] = {sources.first_pass, sources.second_pass, sources.properties, sources.partition_registration};
    const uint32_t sizes[] = {6, 6, 8, 31};
    for (uint32_t i = 0; i < 4; ++i) {
        unsigned char *entry = (void *)(uintptr_t)entries[i];
        for (uint32_t n = 0; n < sizes[i]; ++n) {
            entry[n] ^= 0x80;
            assert(!kwengine_initial_map_hook_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
            entry[n] ^= 0x80;
        }
        assert(kwengine_initial_map_hook_sources(&sources));
    }
    const uint32_t returns[] = {sources.ordinary_return, sources.bridge_return};
    for (uint32_t i = 0; i < 2; ++i) {
        assert(!kwengine_patch_site_valid(returns[i], 5)); // Not a conventional hook entry.
        unsigned char *call = (void *)(uintptr_t)(returns[i] - 5);
        call[1] ^= 1;
        assert(!kwengine_initial_map_hook_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
        call[1] ^= 1;
        call[0] = 0xe9;
        assert(!kwengine_initial_map_hook_sources(&sources));
        call[0] = 0xe8;
        assert(kwengine_initial_map_hook_sources(&sources));
    }
    puts("PASS initial creation pass/property prefixes and exact call-target provenance; return sites are not hook entries");
}
static void replay_view_bindings(void)
{
    struct kwengine_replay_view_bindings sources, zero = {0};
    assert(!kwengine_replay_view_sources(NULL) && kwengine_replay_view_sources(&sources));
    const uint32_t entries[] = {sources.select_observed_player, sources.ghost_select_player};
    const uint32_t sizes[] = {6, 8};
    for (uint32_t i = 0; i < 2; ++i) {
        unsigned char *entry = (void *)(uintptr_t)entries[i];
        for (uint32_t n = 0; n < sizes[i]; ++n) {
            entry[n] ^= 0x80;
            assert(!kwengine_replay_view_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
            entry[n] ^= 0x80;
        }
        assert(kwengine_replay_view_sources(&sources));
    }
    uint32_t *slot = (void *)(kwengine_va(0xa9a99cU) + 0x14), method = *slot;
    *slot = sources.select_observed_player;
    assert(!kwengine_replay_view_sources(&sources) && !memcmp(&sources, &zero, sizeof(sources)));
    *slot = method;
    assert(kwengine_replay_view_sources(&sources));
    puts("PASS stock replay/observer viewpoint sources, native manager slot and changed-entry refusal; no selection or native calls");
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *input = fopen(argv[1], "rb");
    assert(input && !fseek(input, 0, SEEK_END));
    const long length = ftell(input);
    assert(length >= 4096 && !fseek(input, 0, SEEK_SET));
#ifdef _WIN32
    /* Keep the large raw-file buffer out of the synthetic game's low VA range.
     * A 32-bit CRT heap allocation here can otherwise occupy 0x400000 before
     * the mapped-image fixture reserves it. Never relocate/free Wine heaps. */
    unsigned char *raw = VirtualAlloc((void *)0x30000000U, (size_t)length, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    unsigned char *raw = malloc((size_t)length);
#endif
    assert(raw && fread(raw, 1, (size_t)length, input) == (size_t)length);
    fclose(input);
    const uint32_t nt = engine_u32(raw + 60);
    assert((uint64_t)nt + 256 <= (uint64_t)length);
    const uint32_t size = engine_u32(raw + nt + 80), headers = engine_u32(raw + nt + 84);
    const uint32_t table = nt + 24 + engine_u16(raw + nt + 20);
    const uint32_t sections = engine_u16(raw + nt + 6);
    assert(headers <= (uint64_t)length && (uint64_t)table + sections * 40 <= headers);
#ifdef _WIN32
    unsigned char *image = VirtualAlloc((void *)0x400000U, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    unsigned char *image = mmap((void *)0x400000U, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
#endif
    if (image != (void *)0x400000U) { /* Fixture EXE is linked at 0x20000000. */
        fputs("UNAVAILABLE: fixture cannot reserve the complete synthetic game range at 0x400000; no source check ran\n", stderr);
#ifdef _WIN32
        assert(VirtualFree(raw, 0, MEM_RELEASE));
#else
        free(raw);
#endif
        return 77;
    }
    memcpy(image, raw, headers);
    for (uint32_t i = 0; i < sections; ++i) {
        const unsigned char *section = raw + table + 40 * i;
        const uint32_t at = engine_u32(section + 12), bytes = engine_u32(section + 16), offset = engine_u32(section + 20);
        assert((uint64_t)at + bytes <= size && (uint64_t)offset + bytes <= (uint64_t)length);
        memcpy(image + at, raw + offset, bytes);
    }
    for (size_t i = 1; i < ENGINE_COUNT; ++i)
        assert(engine_symbols[i - 1].reference < engine_symbols[i].reference);
    struct kwengine_map_source_bindings bindings, zero = {0};
    struct kwengine_object_fog_bindings fog, fog_zero = {0};
    memset(&bindings, 0xff, sizeof(bindings));
    assert(!kwengine_map_sources(&bindings) && !memcmp(&bindings, &zero, sizeof(bindings)));
    memset(&fog, 0xff, sizeof(fog));
    assert(!kwengine_object_fog_sources(&fog) && !memcmp(&fog, &fog_zero, sizeof(fog)));
    ready(image, size);
    lobby_ui_bindings();
    assert(kwengine_map_sources(&bindings));
    printf("resolved holder=%08x map-vtable=%08x ghost-global=%08x manager-vtable=%08x ghost-vtable=%08x\n", bindings.authored_holder,
           bindings.authored_vtable, bindings.ghost_manager_global, bindings.ghost_manager_vtable, bindings.ghost_vtable);
    named_bindings(image, size);
    global_bindings(image, size);
    rtti_bindings(image, size);
    object_fog_bindings();
    hook_bindings();
    initial_map_bindings();
    replay_view_bindings();
    beacon_bindings();
    /* One actual incomplete-cache integration check, not a full resolution
     * rerun for every isolated helper mutation above. */
    const uint32_t radius = bindings.resource_keys[KWENGINE_RESOURCE_RADIUS];
    unsigned char *radius_name = (void *)(uintptr_t)engine_u32((void *)(uintptr_t)(radius + 4));
    radius_name[0] ^= 0x20;
    denied(image, size);
    radius_name[0] ^= 0x20;
    ready(image, size);
    kwengine_reset();
    assert(!kwengine_map_sources(&bindings) && !memcmp(&bindings, &zero, sizeof(bindings)));
    assert(!kwengine_object_fog_sources(&fog) && !memcmp(&fog, &fog_zero, sizeof(fog)));
    printf("PASS %s: %zu shared bindings; named relocation/duplicates/missing, lazy-key changes, globals, RTTI and data authority; no native calls\n",
           argv[1], ENGINE_COUNT);
#ifdef _WIN32
    assert(VirtualFree(image, 0, MEM_RELEASE));
    assert(VirtualFree(raw, 0, MEM_RELEASE));
#else
    assert(!munmap(image, size));
    free(raw);
#endif
    return 0;
}
