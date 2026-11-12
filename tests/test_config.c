/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#include <kwengine/config.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct example {
    uint32_t flag, number, scale, rgb, mode;
};
static const struct kwengine_config_field fields[] = {{"flag", offsetof(struct example, flag), KWENGINE_CONFIG_BOOL, 0, 1, NULL},
                                                      {"number", offsetof(struct example, number), KWENGINE_CONFIG_U32, 1, 100, NULL},
                                                      {"scale", offsetof(struct example, scale), KWENGINE_CONFIG_SCALE, 50, 200, NULL},
                                                      {"rgb", offsetof(struct example, rgb), KWENGINE_CONFIG_RGB, 0, 0xffffff, NULL},
                                                      {"mode", offsetof(struct example, mode), KWENGINE_CONFIG_ENUM, 0, 2, "stock|fast|off"},
                                                      {"alias", offsetof(struct example, number), KWENGINE_CONFIG_U32, 1, 100, NULL}};
static int parse(const char *text, struct example *out, int legacy, size_t *line)
{
    return kwengine_config_parse(text, strlen(text), out, sizeof(*out), fields, 6, legacy, line);
}
int main(void)
{
    struct example value = {0};
    size_t line = 99;
    assert(parse("# comment\r\n flag = true\r\nnumber=100\nscale=1.35\nrgb=160, 160,160\nmode=off\n", &value, 0, &line));
    assert(value.flag == 1 && value.number == 100 && value.scale == 135 && value.rgb == 0xa0a0a0 && value.mode == 2 && !line);
    const char *invalid[] = {"flag=1",       "number=0",    "number=101", "number=42949672960",    "scale=0.49",         "scale=2.01",
                             "scale=1.001",  "rgb=256,0,0", "rgb=1,2",    "mode=unknown",          "number=2\nnumber=3", "number=2\nalias=3",
                             "unknown=true", "flag",        "=true",      "flag=true\nflag=false", "flag=TRUE",          "flag=true\0x"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid) - 1; ++i)
        assert(!parse(invalid[i], &value, 0, &line));
    assert(!kwengine_config_parse("flag=true\0x", 11, &value, sizeof(value), fields, 6, 0, &line));
    assert(!kwengine_config_parse("\xff", 1, &value, sizeof(value), fields, 6, 0, &line));
    assert(parse("controller_host=127.0.0.1\nnumber=5", &value, 1, &line) && value.number == 5);
    assert(parse("", &value, 0, &line));
    assert(!parse("flag=true\nnumber=bad", &value, 0, &line) && line == 2);
    puts("PASS neutral config: types/ranges, UTF-8, aliases, duplicates, legacy filtering, line numbers");
    return 0;
}
