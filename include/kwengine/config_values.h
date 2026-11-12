/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_CONFIG_VALUES_H
#define KWENGINE_CONFIG_VALUES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int kwengine_config_valid_utf8(const unsigned char *, size_t);
int kwengine_config_equals(const unsigned char *, size_t, const char *);
int kwengine_config_u32(const unsigned char *, size_t, uint32_t *);
/* Unsigned decimal with up to two fractional digits, returned in hundredths.
 * A feature's allowed range is enforced by its schema, not this primitive. */
int kwengine_config_fixed100(const unsigned char *, size_t, uint32_t *);
int kwengine_config_bool(const unsigned char *, size_t, int *);
int kwengine_config_copy_value(char *, size_t, const unsigned char *, size_t);
void kwengine_config_trim(const unsigned char **, const unsigned char **);
int kwengine_config_rgb(const unsigned char *, size_t, uint32_t *);
#ifdef __cplusplus
}
#endif
#endif
