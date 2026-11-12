/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#include <kwengine/config.h>
#include <kwengine/config_values.h>
#include <string.h>
int kwengine_config_valid_utf8(const unsigned char *text, size_t size)
{
    size_t index = 0U;
    while (index < size) {
        uint32_t value;
        uint32_t minimum;
        unsigned int trailing;
        unsigned char first = text[index];
        unsigned int offset;

        if (first == 0U) {
            return 0;
        }
        if (first < 0x80U) {
            ++index;
            continue;
        }
        if ((first & 0xe0U) == 0xc0U) {
            trailing = 1U;
            value = first & 0x1fU;
            minimum = 0x80U;
        } else if ((first & 0xf0U) == 0xe0U) {
            trailing = 2U;
            value = first & 0x0fU;
            minimum = 0x800U;
        } else if ((first & 0xf8U) == 0xf0U) {
            trailing = 3U;
            value = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return 0;
        }
        if (index + trailing >= size) {
            return 0;
        }
        for (offset = 1U; offset <= trailing; ++offset) {
            unsigned char next = text[index + offset];
            if ((next & 0xc0U) != 0x80U) {
                return 0;
            }
            value = (value << 6U) | (next & 0x3fU);
        }
        if (value < minimum || value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU)) {
            return 0;
        }
        index += trailing + 1U;
    }
    return 1;
}

int kwengine_config_equals(const unsigned char *key, size_t size, const char *expected)
{
    size_t expected_size = strlen(expected);
    return size == expected_size && memcmp(key, expected, size) == 0;
}

int kwengine_config_u32(const unsigned char *value, size_t size, uint32_t *output)
{
    uint32_t parsed = 0U;
    size_t index;
    if (size == 0U) {
        return 0;
    }
    for (index = 0U; index < size; ++index) {
        uint32_t digit;
        if (value[index] < '0' || value[index] > '9') {
            return 0;
        }
        digit = (uint32_t)(value[index] - '0');
        if (parsed > (UINT32_MAX - digit) / 10U) {
            return 0;
        }
        parsed = parsed * 10U + digit;
    }
    *output = parsed;
    return 1;
}

/* Decimal multiplier with up to two fractional digits; no locale or exponent. */
int kwengine_config_fixed100(const unsigned char *value, size_t size, uint32_t *output)
{
    uint32_t whole, fraction = 0;
    size_t dot = 0, digits;
    while (dot < size && value[dot] != '.')
        ++dot;
    if (!kwengine_config_u32(value, dot, &whole))
        return 0;
    if (dot < size) {
        digits = size - dot - 1U;
        if (!digits || digits > 2U || !kwengine_config_u32(value + dot + 1U, digits, &fraction))
            return 0;
        if (digits == 1U)
            fraction *= 10U;
    }
    if (whole > (UINT32_MAX - fraction) / 100U)
        return 0;
    whole = whole * 100U + fraction;
    *output = whole;
    return 1;
}

int kwengine_config_bool(const unsigned char *value, size_t size, int *output)
{
    if (kwengine_config_equals(value, size, "true")) {
        *output = 1;
        return 1;
    }
    if (kwengine_config_equals(value, size, "false")) {
        *output = 0;
        return 1;
    }
    return 0;
}

void kwengine_config_trim(const unsigned char **start, const unsigned char **end)
{
    while (*start < *end && (**start == ' ' || **start == '\t')) {
        ++*start;
    }
    while (*end > *start && ((*end)[-1] == ' ' || (*end)[-1] == '\t')) {
        --*end;
    }
}

/* Human-readable R,G,B channels; whitespace around each channel is allowed. */
int kwengine_config_rgb(const unsigned char *value, size_t size, uint32_t *output)
{
    const unsigned char *start = value, *end = value + size;
    uint32_t rgb = 0, channel;
    unsigned i;
    for (i = 0; i < 3; ++i) {
        const unsigned char *separator = start, *component = start, *limit;
        while (separator < end && *separator != ',')
            ++separator;
        limit = separator;
        kwengine_config_trim(&component, &limit);
        if (!kwengine_config_u32(component, (size_t)(limit - component), &channel) || channel > 255U)
            return 0;
        rgb = (rgb << 8U) | channel;
        if (i < 2) {
            if (separator == end)
                return 0;
            start = separator + 1;
        } else if (separator != end)
            return 0;
    }
    *output = rgb;
    return 1;
}

int kwengine_config_copy_value(char *output, size_t output_size, const unsigned char *value, size_t size)
{
    size_t index;
    if (size >= output_size) {
        return 0;
    }
    for (index = 0U; index < size; ++index) {
        if (value[index] < 0x20U || value[index] == 0x7fU) {
            return 0;
        }
    }
    memcpy(output, value, size);
    output[size] = '\0';
    return 1;
}

int kwengine_config_parse(const void *data, size_t size, void *config, size_t config_size, const struct kwengine_config_field *fields, size_t count,
                          int allow_unknown, size_t *error_line)
{
    const unsigned char *input = data;
    uint64_t seen = 0;
    size_t offset = 0, line = 0;
    if (error_line)
        *error_line = 0;
    if ((!data && size) || !config || !fields || count > 64 || size > 16384 || !kwengine_config_valid_utf8(input, size))
        return 0;
    while (offset < size) {
        const unsigned char *start = input + offset, *end, *equals, *key_end, *value;
        ++line;
        while (offset < size && input[offset] != '\n')
            ++offset;
        end = input + offset;
        if (offset < size)
            ++offset;
        if (end > start && end[-1] == '\r')
            --end;
        kwengine_config_trim(&start, &end);
        if (start == end || *start == '#' || *start == ';')
            continue;
        equals = memchr(start, '=', (size_t)(end - start));
        if (!equals)
            goto invalid;
        key_end = equals;
        value = equals + 1;
        kwengine_config_trim(&start, &key_end);
        kwengine_config_trim(&value, &end);
        if (start == key_end)
            goto invalid;
        size_t index;
        for (index = 0; index < count; ++index)
            if (kwengine_config_equals(start, (size_t)(key_end - start), fields[index].key))
                break;
        if (index == count) {
            if (allow_unknown)
                continue;
            goto invalid;
        }
        const struct kwengine_config_field *field = &fields[index];
        if (field->offset > config_size || config_size - field->offset < sizeof(uint32_t) || (seen & (UINT64_C(1) << index)))
            goto invalid;
        // Aliases sharing an output slot are duplicates, too.
        for (size_t previous = 0; previous < count; ++previous)
            if ((seen & (UINT64_C(1) << previous)) && fields[previous].offset == field->offset)
                goto invalid;
        seen |= UINT64_C(1) << index;
        uint32_t number = 0;
        size_t length = (size_t)(end - value);
        if (field->kind == KWENGINE_CONFIG_BOOL) {
            int boolean;
            if (!kwengine_config_bool(value, length, &boolean))
                goto invalid;
            number = (uint32_t)boolean;
        } else if (field->kind == KWENGINE_CONFIG_RGB) {
            if (!kwengine_config_rgb(value, length, &number))
                goto invalid;
        } else if (field->kind == KWENGINE_CONFIG_SCALE) {
            if (!kwengine_config_fixed100(value, length, &number))
                goto invalid;
        } else if (field->kind == KWENGINE_CONFIG_U32) {
            if (!kwengine_config_u32(value, length, &number))
                goto invalid;
        } else if (field->kind == KWENGINE_CONFIG_ENUM) {
            const char *choice = field->choices;
            if (!choice)
                goto invalid;
            for (;;) {
                const char *separator = strchr(choice, '|');
                size_t choice_size = separator ? (size_t)(separator - choice) : strlen(choice);
                if (length == choice_size && !memcmp(value, choice, length))
                    break;
                if (!separator)
                    goto invalid;
                choice = separator + 1;
                ++number;
            }
        } else
            goto invalid;
        if (number < field->minimum || number > field->maximum)
            goto invalid;
        memcpy((unsigned char *)config + field->offset, &number, sizeof(number));
    }
    return 1;
invalid:
    if (error_line)
        *error_line = line;
    return 0;
}
