/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
/* Included by the retained lifecycle fixture, which supplies two real plugin
 * contexts, owner-thread controls and an aligned native byte range. */
#include <kwf/patch_query.h>
static void check_patch_query(const struct kwf_context *context, int prepared)
{
    const struct kwf_patch_query *api = context->host->service(context->owner, KWF_PATCH_QUERY_SERVICE, 1);
    struct kwf_patch_info info = {0};
    unsigned char original[4] = {0};
    info.size = sizeof(info);
    assert(api && api->size >= sizeof(*api) && api->version == 1);
    assert(!context->host->service(context->owner, KWF_PATCH_QUERY_SERVICE, 2));
    assert(context->host->publish(context->owner, KWF_PATCH_QUERY_SERVICE, 1, api) == KWF_CONFLICT);
    uint32_t found = api->find(context->owner, (uintptr_t)bytes, 4, &info);
    assert(found == (prepared ? KWF_OK : KWF_UNSUPPORTED));
    if (prepared) {
        assert(info.address == (uintptr_t)bytes && info.length == 4 && !info.written && info.provider_id);
        assert(api->original(context->owner, (uintptr_t)bytes, 4, original) == KWF_OK);
        assert(!memcmp(original, bytes, 4));
        assert(api->original(context->owner, (uintptr_t)bytes + 3, 2, original) == KWF_CONFLICT);
    }
    assert(api->find(NULL, (uintptr_t)bytes, 4, &info) == KWF_ERROR);
    assert(api->find(context->owner, UINTPTR_MAX, 4, &info) == KWF_ERROR);
    info.size = 0;
    assert(api->find(context->owner, (uintptr_t)bytes, 4, &info) == KWF_ERROR);
    info.size = sizeof(info);
    thread_id = 2;
    assert(api->find(context->owner, (uintptr_t)bytes, 4, &info) == KWF_BAD_PHASE);
    thread_id = 1;
}
static void check_patch_query_after_activation(const struct kwf_context *context)
{
    const struct kwf_patch_query *api = context->host->service(context->owner, KWF_PATCH_QUERY_SERVICE, 1);
    struct kwf_patch_info info = {0};
    unsigned char original[4] = {0};
    const unsigned char expected[] = {1, 2, 3, 4};
    info.size = sizeof(info);
    assert(api->find(context->owner, (uintptr_t)bytes, 4, &info) == KWF_OK);
    assert(api->original(context->owner, (uintptr_t)bytes, 4, original) == KWF_OK);
    assert(!memcmp(original, expected, 4));
    if (mode == 1 || mode == 2)
        assert(info.provider_state == 2 && info.written);
    if (mode == 5 || mode == 10)
        assert(info.provider_state == 0 && !info.written);
    if (mode == 6 || mode == 8)
        assert(info.provider_state == 0 && info.written); /* Foreign bytes were not overwritten by rollback. */
}
