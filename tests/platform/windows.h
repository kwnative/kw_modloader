/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#ifndef KWENGINE_HOOK_CONTEXT_TEST_WINDOWS_H
#define KWENGINE_HOOK_CONTEXT_TEST_WINDOWS_H
/* Only the OS surface used by the isolated hook-context test. */
#include <stdint.h>
typedef uint32_t DWORD;
DWORD GetLastError(void);
void SetLastError(DWORD);
#endif
