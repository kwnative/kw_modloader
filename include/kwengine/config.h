/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_CONFIG_H
#define KWENGINE_CONFIG_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Schema-neutral INI primitives. Feature names, defaults and struct layouts
 * are supplied by the plugin; no plugin configuration is defined by the SDK. */
enum kwengine_config_kind { KWENGINE_CONFIG_BOOL, KWENGINE_CONFIG_U32, KWENGINE_CONFIG_SCALE, KWENGINE_CONFIG_RGB, KWENGINE_CONFIG_ENUM };
struct kwengine_config_field {
    const char *key;
    size_t offset;
    enum kwengine_config_kind kind;
    uint32_t minimum, maximum;
    const char *choices;
};
int kwengine_config_parse(const void *data, size_t size, void *config, size_t config_size, const struct kwengine_config_field *fields, size_t count,
                          int allow_unknown, size_t *error_line);
#ifdef __cplusplus
}
#endif
#endif
