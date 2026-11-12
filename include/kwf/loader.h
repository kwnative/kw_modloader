/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: MIT
 */
#ifndef KWF_LOADER_H
#define KWF_LOADER_H
#include <kwf/plugin.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Call outside DllMain, before the game runs. The selected role is immutable.
 * A managed bot uses HEADLESS even when its own renderer is enabled, so it
 * does not start rendered human-controller plugins recursively. */
KWF_EXPORT uint32_t KWF_CALL KwFrameworkStart(uint32_t role);
#ifdef _WIN32
/* CreateRemoteThread-compatible adapter. The parameter is the numeric role
 * cast to a pointer, not a pointer to target memory. No plugin-specific API. */
KWF_EXPORT uint32_t __stdcall KwFrameworkStartInjected(void *role);
#endif
#ifdef __cplusplus
}
#endif
#endif
