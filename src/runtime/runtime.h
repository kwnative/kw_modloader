/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#ifndef KWF_RUNTIME_H
#define KWF_RUNTIME_H
#include <kwf/plugin.h>
#ifdef _WIN32
#include <windows.h>
KWF_EXPORT uint32_t KWF_CALL KwFrameworkInitialize(HMODULE loader, uint32_t role);
KWF_EXPORT uint32_t KWF_CALL KwFrameworkStatus(void);
#endif
#endif
