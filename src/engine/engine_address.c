/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#define KWENGINE_USE_RUNTIME_RESOLVER
#include <kwengine/engine_address.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

enum {
    ENGINE_PATTERN_SELF,
    ENGINE_PATTERN_ABSOLUTE,
    ENGINE_PATTERN_RELATIVE,
    ENGINE_RTTI,
    ENGINE_IMPORT,
    ENGINE_ALIAS,
    ENGINE_VIRTUAL,
    ENGINE_NAMED_DATA
};
struct engine_symbol {
    uint32_t reference, kind, argument, offset, instruction_size;
    const char *name;
    const unsigned char *bytes, *mask;
    uint32_t length;
    const unsigned char *prefix, *prefix_mask;
    uint32_t prefix_length;
};
#include "engine_symbols.inc"
#define ENGINE_COUNT (sizeof(engine_symbols) / sizeof(engine_symbols[0]))
struct engine_section {
    uint32_t start, size, flags;
};
static struct {
    const unsigned char *image;
    size_t size;
    uint32_t base, import_rva, section_count;
    struct engine_section sections[96];
    uint32_t addresses[ENGINE_COUNT];
    /* Immutable resolved-address index, not a code-byte/permission cache.
     * Stable symbol order preserves aliases with different prefix lengths. */
    uint32_t prefix_order[ENGINE_COUNT], prefix_count;
    int ready;
} engine;
static void engine_index_prefixes(void)
{
    engine.prefix_count = 0;
    for (uint32_t index = 0; index < ENGINE_COUNT; ++index) {
        if (!engine.addresses[index] || !engine_symbols[index].prefix_length)
            continue;
        uint32_t position = engine.prefix_count++;
        while (position && engine.addresses[engine.prefix_order[position - 1]] > engine.addresses[index]) {
            engine.prefix_order[position] = engine.prefix_order[position - 1];
            --position;
        }
        engine.prefix_order[position] = index;
    }
}
static uint32_t engine_prefix_begin(uint32_t address)
{
    uint32_t low = 0, high = engine.prefix_count;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2;
        if (engine.addresses[engine.prefix_order[middle]] < address)
            low = middle + 1;
        else
            high = middle;
    }
    return low;
}
static uint16_t engine_u16(const void *p)
{
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}
static uint32_t engine_u32(const void *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}
static int engine_span(uint32_t at, size_t n)
{
    return at <= engine.size && n <= engine.size - at;
}
static int engine_va_span(uint32_t at, size_t n)
{
    return at >= engine.base && engine_span(at - engine.base, n);
}
static int engine_readable(uint32_t at, size_t n)
{
    uint32_t i;
    if (!engine_span(at, n))
        return 0;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if ((s->flags & 0x40000000U) && at >= s->start && at - s->start <= s->size && n <= s->size - (at - s->start))
            return 1;
    }
    return 0;
}
static int engine_executable(uint32_t address)
{
    uint32_t i;
    if (address < engine.base || (size_t)(address - engine.base) >= engine.size)
        return 0;
    address -= engine.base;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if ((s->flags & 0x20000000U) && address >= s->start && address - s->start < s->size)
            return 1;
    }
    return 0;
}
static int engine_mapped_readable(const void *pointer, size_t size)
{
#ifdef _WIN32
    uintptr_t at = (uintptr_t)pointer, end = at + size;
    MEMORY_BASIC_INFORMATION info;
    if (end < at)
        return 0;
    while (at < end) {
        if (!VirtualQuery((const void *)at, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return 0;
        DWORD p = info.Protect & 0xffU;
        if (p != PAGE_READONLY && p != PAGE_READWRITE && p != PAGE_WRITECOPY && p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE &&
            p != PAGE_EXECUTE_WRITECOPY)
            return 0;
        uintptr_t next = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next <= at)
            return 0;
        at = next;
    }
#else
    (void)pointer;
    (void)size;
#endif
    return 1;
}
static int engine_masked(const unsigned char *actual, const unsigned char *bytes, const unsigned char *mask, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
        if ((actual[i] & mask[i]) != (bytes[i] & mask[i]))
            return 0;
    return 1;
}
static uint32_t engine_unique_pattern(const struct engine_symbol *symbol)
{
    uint32_t i, found = 0;
    size_t first = 0;
    while (first < symbol->length && !symbol->mask[first])
        ++first;
    if (first == symbol->length)
        return 0;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        size_t offset = 0, limit;
        if ((s->flags & 0x60000000U) != 0x60000000U || s->size < symbol->length)
            continue;
        limit = s->size - symbol->length;
        while (offset <= limit) {
            const unsigned char *hit = memchr(engine.image + s->start + offset + first, symbol->bytes[first], limit - offset + 1U);
            uint32_t at;
            if (!hit)
                break;
            offset = (size_t)(hit - (engine.image + s->start)) - first;
            if (engine_masked(engine.image + s->start + offset, symbol->bytes, symbol->mask, symbol->length)) {
                at = s->start + (uint32_t)offset;
                if (found)
                    return 0;
                found = at;
            }
            ++offset;
        }
    }
    return found;
}
static int engine_string(uint32_t at, const char *expected)
{
    size_t n = strlen(expected) + 1U;
    return engine_readable(at, n) && !memcmp(engine.image + at, expected, n);
}
static uint32_t engine_rtti(const struct engine_symbol *symbol)
{
    uint32_t i, j, k, descriptor = 0, locator = 0, result = 0;
    size_t n = strlen(symbol->name) + 1U;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if (!(s->flags & 0x40000000U) || (s->flags & 0x20000000U) || s->size < n)
            continue;
        for (j = 8; j + n <= s->size; ++j)
            if (engine.image[s->start + j] == '.' && !memcmp(engine.image + s->start + j, symbol->name, n)) {
                if (descriptor)
                    return 0;
                descriptor = engine.base + s->start + j - 8U;
            }
    }
    if (!descriptor)
        return 0;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if (!(s->flags & 0x40000000U) || (s->flags & 0xa0000000U) || s->size < 20)
            continue;
        for (j = 0; j + 20 <= s->size; j += 4) {
            const unsigned char *p = engine.image + s->start + j;
            if (engine_u32(p) != 0 || engine_u32(p + 4) != symbol->argument || engine_u32(p + 8) != 0 || engine_u32(p + 12) != descriptor ||
                !engine_va_span(engine_u32(p + 16), 16))
                continue;
            if (locator)
                return 0;
            locator = engine.base + s->start + j;
        }
    }
    if (!locator)
        return 0;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if (!(s->flags & 0x40000000U) || (s->flags & 0xa0000000U) || s->size < 8)
            continue;
        for (k = 0; k + 8 <= s->size; k += 4)
            if (engine_u32(engine.image + s->start + k) == locator && engine_executable(engine_u32(engine.image + s->start + k + 4))) {
                if (result)
                    return 0;
                result = engine.base + s->start + k + 4;
            }
    }
    return result;
}
static uint32_t engine_import(const struct engine_symbol *symbol)
{
    uint32_t descriptor = engine.import_rva, result = 0;
    unsigned count;
    for (count = 0; count < 256 && engine_readable(descriptor, 20); ++count, descriptor += 20) {
        const unsigned char *d = engine.image + descriptor;
        uint32_t names = engine_u32(d), iat = engine_u32(d + 16), i;
        if (!names && !iat)
            break;
        if (!names)
            continue;
        for (i = 0; i < 65536 && engine_readable(names + 4 * i, 4) && engine_span(iat + 4 * i, 4); ++i) {
            uint32_t name = engine_u32(engine.image + names + 4 * i);
            if (!name)
                break;
            if (!(name & 0x80000000U) && name <= UINT32_MAX - 2U && engine_string(name + 2, symbol->name)) {
                if (result)
                    return 0;
                result = engine.base + iat + 4 * i;
            }
        }
    }
    return result;
}
/* Native named data wrapper: unique literal plus unique aligned writable-data
 * reference to it. offset locates the name pointer within instruction_size
 * bytes of data. No key creation, name hashing, engine call or reference delta.
 * The mutable value preceding a name is deliberately not used as a signature.
 */
static uint32_t engine_named_data(const struct engine_symbol *symbol)
{
    uint32_t literal = 0, result = 0;
    const size_t bytes = strlen(symbol->name) + 1U;
    if (bytes <= 1 || (symbol->offset & 3U) || symbol->instruction_size < 4 || symbol->offset > symbol->instruction_size - 4)
        return 0;
    for (uint32_t i = 0; i < engine.section_count; ++i) {
        const struct engine_section *section = &engine.sections[i];
        if (!(section->flags & 0x40000000U) || (section->flags & 0x20000000U) || section->size < bytes)
            continue;
        for (size_t at = 0; at <= section->size - bytes; ++at)
            if (engine.image[section->start + at] == (unsigned char)symbol->name[0] &&
                !memcmp(engine.image + section->start + at, symbol->name, bytes)) {
                if (literal)
                    return 0;
                literal = engine.base + section->start + (uint32_t)at;
            }
    }
    if (!literal)
        return 0;
    for (uint32_t i = 0; i < engine.section_count; ++i) {
        const struct engine_section *section = &engine.sections[i];
        if ((section->flags & 0xe0000000U) != 0xc0000000U || (section->start & 3U) || section->size < symbol->instruction_size)
            continue;
        for (size_t at = 0; at <= section->size - symbol->instruction_size; at += 4)
            if (engine_u32(engine.image + section->start + at + symbol->offset) == literal) {
                if (result)
                    return 0;
                result = engine.base + section->start + (uint32_t)at;
            }
    }
    return result;
}
void kwengine_reset(void)
{
    memset(&engine, 0, sizeof(engine));
}
/* Alias/virtual discovery needs partial entries internally. Never publish
 * that capability through the public lookup, even during resolution. */
static uintptr_t engine_lookup(uintptr_t reference)
{
    size_t low = 0, high = ENGINE_COUNT;
    if (reference == 0x400000U)
        return engine.base;
    while (low < high) {
        size_t middle = low + (high - low) / 2U;
        uint32_t key = engine_symbols[middle].reference;
        if (reference == key)
            return engine.addresses[middle];
        if (reference < key)
            high = middle;
        else
            low = middle + 1U;
    }
    return 0;
}
uintptr_t kwengine_va(uintptr_t reference)
{
    return engine.ready ? engine_lookup(reference) : 0;
}
uint32_t kwengine_rva(uint32_t reference)
{
    if (!engine.ready)
        return 0;
    uintptr_t value = kwengine_va((uintptr_t)reference + 0x400000U);
    return value >= engine.base ? (uint32_t)(value - engine.base) : 0;
}
int kwengine_resolve(const void *image, size_t available, struct kwengine_resolution *status)
{
    const unsigned char *b = image;
    uint32_t nt, optional, table, i, pass;
    size_t index;
    kwengine_reset();
    /* Public lookups and all patch guards stay closed until complete success. */
    if (!status)
        return 0;
    memset(status, 0, sizeof(*status));
    if (!b || available < 64 || engine_u16(b) != 0x5a4d)
        return 0;
    nt = engine_u32(b + 60);
    if (nt > available || available - nt < 24 || engine_u32(b + nt) != 0x4550 || engine_u16(b + nt + 4) != 0x14c)
        return 0;
    optional = engine_u16(b + nt + 20);
    if (optional < 112 || available - nt - 24 < optional || engine_u16(b + nt + 24) != 0x10b)
        return 0;
    engine.size = engine_u32(b + nt + 24 + 56);
    if (engine.size > available || engine.size < 4096 || (uintptr_t)b > UINT32_MAX - engine.size)
        return 0;
    engine.image = b;
    engine.base = (uint32_t)(uintptr_t)b;
    engine.import_rva = engine_u32(b + nt + 24 + 104);
    engine.section_count = engine_u16(b + nt + 6);
    table = nt + 24 + optional;
    if (!engine.section_count || engine.section_count > 96 || !engine_span(table, 40U * engine.section_count))
        return 0;
    for (i = 0; i < engine.section_count; ++i) {
        const unsigned char *s = b + table + 40 * i;
        struct engine_section *out = &engine.sections[i];
        out->start = engine_u32(s + 12);
        out->size = engine_u32(s + 8);
        out->flags = engine_u32(s + 36);
        if (!engine_span(out->start, out->size))
            return 0;
        if (!engine_mapped_readable(b + out->start, out->size))
            out->flags = 0;
    }
    status->image_size = (uint32_t)engine.size;
    status->expected = ENGINE_COUNT;
    for (pass = 0; pass < 2; ++pass)
        for (index = 0; index < ENGINE_COUNT; ++index) {
            const struct engine_symbol *s = &engine_symbols[index];
            uint32_t at = 0, value = 0;
            if (engine.addresses[index])
                continue;
            if (s->kind == ENGINE_RTTI)
                value = engine_rtti(s);
            else if (s->kind == ENGINE_IMPORT)
                value = engine_import(s);
            else if (s->kind == ENGINE_NAMED_DATA)
                value = engine_named_data(s);
            else if (s->kind == ENGINE_ALIAS || s->kind == ENGINE_VIRTUAL) {
                uintptr_t parent = engine_lookup(s->argument);
                if (parent && parent <= UINT32_MAX - s->offset && engine_va_span((uint32_t)parent + s->offset, 4)) {
                    value = (uint32_t)parent + s->offset;
                    if (s->kind == ENGINE_VIRTUAL)
                        value = engine_u32(b + value - engine.base);
                }
            } else if (s->length && (at = engine_unique_pattern(s)) != 0) {
                if (s->kind == ENGINE_PATTERN_SELF)
                    value = engine.base + at + s->offset;
                else if (s->offset + 4 <= s->length) {
                    value = engine_u32(b + at + s->offset);
                    if (s->kind == ENGINE_PATTERN_RELATIVE)
                        value += engine.base + at + s->instruction_size;
                }
            }
            if (engine_va_span(value, 1))
                engine.addresses[index] = value;
        }
    for (index = 0; index < ENGINE_COUNT; ++index) {
        if (engine.addresses[index])
            ++status->resolved;
        else if (!status->missing_reference)
            status->missing_reference = engine_symbols[index].reference;
    }
    if (status->resolved == status->expected) {
        engine_index_prefixes();
        engine.ready = 1;
    }
    return engine.ready;
}
int kwengine_is_ready(void)
{
    return engine.ready;
}
int kwengine_code_span(uintptr_t address, size_t n)
{
    uint32_t i;
    if (!engine.ready || !n || address < engine.base || address > UINT32_MAX)
        return 0;
    address -= engine.base;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if ((s->flags & 0x60000000U) == 0x60000000U && address >= s->start && address - s->start <= s->size && n <= s->size - (address - s->start))
            return 1;
    }
    return 0;
}
int kwengine_patch_site_valid(uintptr_t address, size_t n)
{
    if (!kwengine_code_span(address, n))
        return 0;
    for (uint32_t position = engine_prefix_begin((uint32_t)address); position < engine.prefix_count; ++position) {
        const uint32_t i = engine.prefix_order[position];
        if (engine.addresses[i] != address)
            break;
        const struct engine_symbol *s = &engine_symbols[i];
        if (s->prefix_length >= n && engine_masked((const unsigned char *)address, s->prefix, s->prefix_mask, n))
            return 1;
    }
    return 0;
}
void kwengine_normalize_code(uintptr_t address, void *data, size_t n)
{
    size_t j;
    unsigned char *bytes = data;
    if (!engine.ready || !data || n > 128 || address > UINT32_MAX || !engine_executable((uint32_t)address))
        return;
    for (uint32_t position = engine_prefix_begin((uint32_t)address); position < engine.prefix_count; ++position) {
        const uint32_t i = engine.prefix_order[position];
        if (engine.addresses[i] != address)
            break;
        const struct engine_symbol *s = &engine_symbols[i];
        if (n > s->prefix_length)
            continue;
        if (!engine_masked(bytes, s->prefix, s->prefix_mask, n))
            return;
        for (j = 0; j < n; ++j)
            bytes[j] = (unsigned char)((bytes[j] & s->prefix_mask[j]) | (s->prefix[j] & ~s->prefix_mask[j]));
        return;
    }
}
int kwengine_memcmp(const void *a, const void *b, size_t n)
{
    unsigned char normalized[128];
    if (engine.ready && n <= sizeof(normalized) && engine_executable((uint32_t)(uintptr_t)a)) {
        memcpy(normalized, a, n);
        kwengine_normalize_code((uintptr_t)a, normalized, n);
        return memcmp(normalized, b, n);
    }
    return memcmp(a, b, n);
}

int kwengine_is_code(uintptr_t address)
{
    return engine.ready && address <= UINT32_MAX && engine_executable((uint32_t)address);
}
int kwengine_is_data(uintptr_t address, size_t size)
{
    uint32_t i;
    if (!engine.ready || address < engine.base || address > UINT32_MAX)
        return 0;
    address -= engine.base;
    for (i = 0; i < engine.section_count; ++i) {
        const struct engine_section *s = &engine.sections[i];
        if ((s->flags & 0x40000000U) && !(s->flags & 0x20000000U) && address >= s->start && address - s->start <= s->size &&
            size <= s->size - (address - s->start))
            return 1;
    }
    return 0;
}
int kwengine_chat_filter_valid(kwengine_read_fn read, void *context)
{
    static const unsigned char native[] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x14, 0x53, 0x56, 0x8b, 0x75,
                                           0x08, 0x8b, 0x06, 0x85, 0xc0, 0x57, 0x89, 0x4d, 0xec};
    uint32_t at = (uint32_t)kwengine_va(0x48c49cU), slot, target = 0;
    const unsigned char *p;
    if (!engine.ready || !engine_va_span(at, sizeof(native)) || !engine_executable(at))
        return 0;
    p = engine.image + at - engine.base;
    if (!memcmp(p, native, sizeof(native)))
        return 1;
    /* Retail protection uses an indirect tail jump. Inspect its actual operand,
     * rather than requiring a particular protection section or dispatch slot. */
    if (p[0] != 0xff || p[1] != 0x25 || p[6] != 0xcc || p[7] != 0xcc || !read)
        return 0;
    slot = engine_u32(p + 2);
    return read(context, slot, &target, 4) && engine_executable(target);
}
int kwengine_default_dependency(kwengine_read_fn read, void *context, uint32_t *value)
{
    uint32_t at = (uint32_t)kwengine_va(0x4be0a8U), i;
    const unsigned char *p;
    if (!value)
        return 0;
    *value = 0;
    if (!engine.ready || !engine_va_span(at, 96) || !engine_executable(at))
        return 0;
    p = engine.image + at - engine.base;
    for (i = 0; i + 10 <= 96; ++i) {
        uint32_t operand, destination;
        if (memcmp(p + i, "\x85\xc0\x75\x05", 4) || p[i + 9] != 0xc3)
            continue;
        operand = engine_u32(p + i + 5);
        if (p[i + 4] == 0xb8) { /* Unprotected getter: mov eax, default_object. */
            if (!kwengine_is_data(operand, 8))
                return 0;
            *value = operand;
            return 1;
        }
        if (p[i + 4] != 0xe9 || !read)
            return 0;
        destination = at + i + 9 + operand;
        if (!engine_va_span(destination, 10) || !engine_executable(destination))
            return 0;
        const unsigned char *thunk = engine.image + destination - engine.base;
        if (thunk[0] != 0xa1 || thunk[5] != 0xe9 || destination + 10 + engine_u32(thunk + 6) != at + i + 9)
            return 0;
        return read(context, engine_u32(thunk + 1), value, 4);
    }
    return 0;
}
