/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#ifndef KWMOD_BUNDLE_STAGE_HPP
#define KWMOD_BUNDLE_STAGE_HPP
#include <filesystem>
#include <string>
#include <vector>
namespace kwmod_bundle
{
std::string sha256(const std::filesystem::path &file);
void stage(const std::vector<std::string> &arguments);
} // namespace kwmod_bundle
#endif
