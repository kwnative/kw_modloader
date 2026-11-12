/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWF_PLUGIN_H
#define KWF_PLUGIN_H
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#define KWF_CALL __cdecl
#define KWF_EXPORT __declspec(dllexport)
#else
#define KWF_CALL
#define KWF_EXPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Windows x86 C ABI. Append fields only within a major version. No owned memory
 * crosses the boundary. All pointers and strings live for the process lifetime. */
#define KWF_ABI_MAJOR 1U
#define KWF_ABI_MINOR 0U
#define KWF_ROLE_RENDERED 1U
#define KWF_ROLE_HEADLESS 2U
#define KWF_OK 0U
#define KWF_ERROR 1U
#define KWF_UNSUPPORTED 2U
#define KWF_CONFLICT 3U
#define KWF_BAD_PHASE 4U
#define KWF_CAP_PATCHES 1U
struct kwf_host;
struct kwf_context {
    uint32_t size, abi_major, abi_minor, role;
    uintptr_t loader_module, plugin_module, game_image;
    uint32_t game_image_size;
    /* UTF-16, absolute paths; directory paths have no trailing separator. */
    const uint16_t *root_directory, *plugin_directory;
    const struct kwf_host *host;
    void *owner;
};
struct kwf_host {
    uint32_t size, abi_major, abi_minor, capabilities;
    void(KWF_CALL *log)(void *owner, const char *message);
    /* Reserve during prepare; write/restore only during activation or later on
     * the startup/engine owner thread. Exact original bytes are retained. */
    uint32_t(KWF_CALL *reserve)(void *owner, uintptr_t address, uint32_t length);
    uint32_t(KWF_CALL *write)(void *owner, uintptr_t address, const void *expected, const void *replacement, uint32_t length);
    uint32_t(KWF_CALL *restore)(void *owner, uintptr_t address, uint32_t length);
    /* Explicitly published process-lifetime services. NULL means unavailable. */
    const void *(KWF_CALL *service)(void *owner, const char *id, uint32_t major);
    uint32_t(KWF_CALL *publish)(void *owner, const char *id, uint32_t major, const void *api);
};
struct kwf_plugin {
    uint32_t size, abi_major, abi_minor, roles, required_capabilities;
    const char *id;
    uint32_t(KWF_CALL *prepare)(const struct kwf_context *context);
    uint32_t(KWF_CALL *activate)(void);
    /* No unload callback: hooked code remains resident until process exit. */
};
typedef const struct kwf_plugin *(KWF_CALL *kwf_query_fn)(void);
/* The only mandatory plugin export. Query/prepare must not change game memory;
 * DllMain must not install hooks, load modules, start threads or perform IO. */
KWF_EXPORT const struct kwf_plugin *KWF_CALL KwPluginQuery(void);
#ifdef __cplusplus
}
#endif
#endif
