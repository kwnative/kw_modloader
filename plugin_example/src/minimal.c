/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#include <kwf/plugin.h>
static const struct kwf_context *host_context;
static uint32_t KWF_CALL prepare(const struct kwf_context *context)
{
    host_context = context;
    context->host->log(context->owner, "independent SDK example prepared");
    return KWF_OK;
}
static uint32_t KWF_CALL activate(void)
{
    host_context->host->log(host_context->owner, "independent SDK example active");
    return KWF_OK;
}
KWF_EXPORT const struct kwf_plugin *KWF_CALL KwPluginQuery(void)
{
    static const struct kwf_plugin plugin = {sizeof(plugin), KWF_ABI_MAJOR, KWF_ABI_MINOR, KWF_ROLE_RENDERED | KWF_ROLE_HEADLESS, 0,
                                             "example",      prepare,       activate};
    return &plugin;
}
