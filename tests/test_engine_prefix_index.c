/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
/* Synthetic mapped-data equivalence test. No game or engine code executes. */
#define _GNU_SOURCE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <time.h>
#endif
#include "../src/engine/engine_address.c"
#include <assert.h>
#include <stdio.h>
enum { fixture_stride = 4096 };

/* Pre-index semantics, intentionally retained as an independent linear oracle. */
static int linear_executable(uint32_t address)
{
    if (address < engine.base)
        return 0;
    address -= engine.base;
    for (uint32_t i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if ((s->flags & 0x20000000U) && address >= s->start && address - s->start < s->size)
            return 1;
    }
    return 0;
}
static void linear_normalize(uintptr_t address, void *data, size_t n)
{
    unsigned char *bytes = data;
    if (!engine.ready || !data || n > 128 || address > UINT32_MAX || !linear_executable((uint32_t)address))
        return;
    for (size_t i = 0; i < ENGINE_COUNT; ++i) {
        const struct engine_symbol *s = &engine_symbols[i];
        if (address != engine.addresses[i] || n > s->prefix_length || !s->prefix_length)
            continue;
        if (!engine_masked(bytes, s->prefix, s->prefix_mask, n))
            return;
        for (size_t j = 0; j < n; ++j)
            bytes[j] = (unsigned char)((bytes[j] & s->prefix_mask[j]) | (s->prefix[j] & ~s->prefix_mask[j]));
        return;
    }
}
static int linear_patch(uintptr_t address, size_t n)
{
    if (!kwengine_code_span(address, n))
        return 0;
    for (size_t i = 0; i < ENGINE_COUNT; ++i) {
        const struct engine_symbol *s = &engine_symbols[i];
        if (address == engine.addresses[i] && s->prefix_length >= n &&
            engine_masked((const unsigned char *)address, s->prefix, s->prefix_mask, n))
            return 1;
    }
    return 0;
}
static uint64_t checks;
static void compare(uintptr_t address, const unsigned char *source, size_t n)
{
    unsigned char expected[fixture_stride], actual[fixture_stride];
    memcpy(expected, source, sizeof(expected));
    memcpy(actual, source, sizeof(actual));
    linear_normalize(address, expected, n);
    kwengine_normalize_code(address, actual, n);
    assert(!memcmp(expected, actual, sizeof(actual)));
    assert(linear_patch(address, n) == kwengine_patch_site_valid(address, n));
    ++checks;
}
static double milliseconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER now, frequency;
    assert(QueryPerformanceCounter(&now) && QueryPerformanceFrequency(&frequency));
    return (double)now.QuadPart * 1000 / (double)frequency.QuadPart;
#else
    struct timespec now;
    assert(!clock_gettime(CLOCK_MONOTONIC, &now));
    return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
#endif
}
int main(void)
{
    const size_t size = 8192 + ENGINE_COUNT * fixture_stride;
    unsigned char *image;
#ifdef _WIN32
    image = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(image);
#else
    image = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    assert(image != MAP_FAILED);
#endif
    assert((uintptr_t)image <= UINT32_MAX - size);
    kwengine_reset();
    engine.image = image;
    engine.base = (uint32_t)(uintptr_t)image;
    engine.size = size;
    engine.section_count = 1;
    engine.sections[0] = (struct engine_section){4096, (uint32_t)size - 4096, 0x60000000};
    for (uint32_t i = 0; i < ENGINE_COUNT; ++i) {
        // Resolved order need not match reference order on compatible images.
        engine.addresses[i] = engine.base + 4096 + (ENGINE_COUNT - i - 1) * fixture_stride;
        assert(engine_symbols[i].prefix_length <= fixture_stride);
        memcpy((void *)(uintptr_t)engine.addresses[i], engine_symbols[i].prefix, engine_symbols[i].prefix_length);
    }
    engine_index_prefixes();
    engine.ready = 1;
    const uint32_t ordinary_count = engine.prefix_count;
    assert(ordinary_count && ordinary_count <= ENGINE_COUNT);
    for (uint32_t i = 0; i < engine.prefix_count; ++i) {
        assert(engine_symbols[engine.prefix_order[i]].prefix_length);
        if (i)
            assert(engine.addresses[engine.prefix_order[i - 1]] <= engine.addresses[engine.prefix_order[i]]);
    }
    for (size_t i = 0; i < ENGINE_COUNT; ++i) {
        unsigned char source[fixture_stride];
        memset(source, 0xa5, sizeof(source));
        const struct engine_symbol *s = &engine_symbols[i];
        for (size_t j = 0; j < s->prefix_length; ++j)
            source[j] = (unsigned char)((s->prefix[j] & s->prefix_mask[j]) | (0x5a & ~s->prefix_mask[j]));
        for (size_t n = 0; n <= 129; ++n)
            compare(engine.addresses[i], source, n);
        for (size_t j = 0; j < s->prefix_length; ++j) {
            source[j] ^= 0xff;
            compare(engine.addresses[i], source, s->prefix_length);
            source[j] ^= 0xff;
        }
        for (size_t j = 0; j < s->prefix_length; ++j)
            if (s->prefix_mask[j]) {
                unsigned char *live = (void *)(uintptr_t)engine.addresses[i];
                assert(kwengine_patch_site_valid(engine.addresses[i], s->prefix_length));
                live[j] ^= s->prefix_mask[j];
                assert(!kwengine_patch_site_valid(engine.addresses[i], s->prefix_length));
                compare(engine.addresses[i], live, s->prefix_length);
                live[j] ^= s->prefix_mask[j];
                assert(kwengine_patch_site_valid(engine.addresses[i], s->prefix_length));
                break;
            }
        compare(engine.addresses[i] + 1, source, 8); // Unregistered executable address.
    }
    unsigned char source[fixture_stride] = {0};
    compare(engine.base, source, 8); // Non-code.
    compare(engine.base + engine.size, source, 8);
    compare(engine.base + engine.size + 4096, source, 8);
    compare(0x10000, source, 8);
    compare(UINTPTR_MAX, source, 8);
    kwengine_normalize_code(engine.addresses[0], NULL, 8);

    // Stable duplicate-address order, different lengths/masks, and first eligible
    // alias mismatch must behave exactly as the linear oracle, not "try next".
    const uint32_t first = engine.prefix_order[0], second = engine.prefix_order[engine.prefix_count - 1];
    const uint32_t saved = engine.addresses[second];
    engine.addresses[second] = engine.addresses[first];
    engine_index_prefixes();
    for (unsigned variant = 0; variant < 3; ++variant) {
        memset(source, variant == 2 ? 0xff : 0, sizeof(source));
        const struct engine_symbol *s = &engine_symbols[variant == 1 ? second : first];
        if (variant != 2)
            memcpy(source, s->prefix, s->prefix_length);
        for (size_t n = 0; n <= 129; ++n)
            compare(engine.addresses[first], source, n);
    }
    engine.addresses[second] = saved;
    engine_index_prefixes();
    assert(engine.prefix_count == ordinary_count);

    // Representative normalization work on registered prefixes. Values are
    // copied every time; timing is advisory and not an engine-speed assertion.
    volatile unsigned checksum = 0;
    const unsigned rounds = 200000;
    const double start = milliseconds();
    for (unsigned i = 0; i < rounds; ++i) {
        const uint32_t index = engine.prefix_order[(i * 17u) % engine.prefix_count];
        memcpy(source, engine_symbols[index].prefix, engine_symbols[index].prefix_length);
        linear_normalize(engine.addresses[index], source, 8);
        checksum += source[0];
    }
    const double middle = milliseconds();
    for (unsigned i = 0; i < rounds; ++i) {
        const uint32_t index = engine.prefix_order[(i * 17u) % engine.prefix_count];
        memcpy(source, engine_symbols[index].prefix, engine_symbols[index].prefix_length);
        kwengine_normalize_code(engine.addresses[index], source, 8);
        checksum += source[0];
    }
    const double end = milliseconds();
    printf("prefix index: symbols=%u prefixes=%u checks=%llu rounds=%u linear_ms=%.3f indexed_ms=%.3f checksum=%u\n",
           (unsigned)ENGINE_COUNT, engine.prefix_count, (unsigned long long)checks, rounds, middle - start, end - middle, checksum);
    engine.ready = 0;
    compare(engine.addresses[first], source, 8);
    kwengine_reset();
    assert(!engine.prefix_count && !kwengine_is_ready());
    assert(!kwengine_patch_site_valid((uintptr_t)image + 4096, 8));
#ifdef _WIN32
    assert(VirtualFree(image, 0, MEM_RELEASE));
#else
    assert(!munmap(image, size));
#endif
    puts("PASS exact normalization/patch-prefix equivalence, aliases, mutations, reset and non-ready guards");
    return 0;
}
