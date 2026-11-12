/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWENGINE_NATIVE_STATUS_H
#define KWENGINE_NATIVE_STATUS_H
#include <stdint.h>
/* Engine-neutral native hook status and calling conventions. This header
 * contains no bot commands, controller configuration or plugin-specific APIs. */
#ifdef _WIN32
#define KWENGINE_EXPORT __declspec(dllexport)
#define KWENGINE_CALL __stdcall
#else
#define KWENGINE_EXPORT
#define KWENGINE_CALL
#endif
#define KWENGINE_NATIVE_PROFILE_ID_SIZE 32U
#define KWENGINE_NATIVE_SHA256_SIZE 65U
#define KWENGINE_RENDER_DISABLED (1U << 0)
enum kwengine_native_state {
    KWENGINE_NATIVE_STATE_LOADED = 1,
    KWENGINE_NATIVE_STATE_INITIALIZING = 2,
    KWENGINE_NATIVE_STATE_PASSIVE_READY = 3,
    KWENGINE_NATIVE_STATE_ACTIVE_READY = 4,
    KWENGINE_NATIVE_STATE_REJECTED_IO = 100,
    KWENGINE_NATIVE_STATE_REJECTED_UNKNOWN_BUILD = 101,
    KWENGINE_NATIVE_STATE_REJECTED_IMAGE = 102,
    KWENGINE_NATIVE_STATE_REJECTED_SIGNATURE = 103,
    KWENGINE_NATIVE_STATE_REJECTED_CONFIG = 104,
    KWENGINE_NATIVE_STATE_REJECTED_HOOK = 105
};

enum kwengine_native_result {
    KWENGINE_NATIVE_OK = 0,
    KWENGINE_NATIVE_ERROR_ALREADY_INITIALIZING = 1,
    KWENGINE_NATIVE_ERROR_IO = 10,
    KWENGINE_NATIVE_ERROR_UNKNOWN_BUILD = 11,
    KWENGINE_NATIVE_ERROR_IMAGE = 12,
    KWENGINE_NATIVE_ERROR_SIGNATURE = 13,
    KWENGINE_NATIVE_ERROR_CONFIG = 14,
    KWENGINE_NATIVE_ERROR_HOOK = 15
};

struct kwengine_native_status {
    uint32_t size;
    uint32_t abi_version;
    uint32_t state;
    uint32_t result;
    uint32_t win32_error;
    uint32_t host_image_base;
    uint32_t host_image_size;
    uint32_t signatures_matched;
    uint32_t signatures_expected;
    char profile_id[KWENGINE_NATIVE_PROFILE_ID_SIZE];
    char host_sha256[KWENGINE_NATIVE_SHA256_SIZE];
};

#endif
