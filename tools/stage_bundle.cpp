/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#include "bundle_stage.hpp"
#include "bundle_format.hpp"
#include <algorithm>
#include <chrono>
#include <clocale>
#include <cwctype>
#include <iostream>
#include <set>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace kwmod_bundle
{
namespace fs = std::filesystem;
static constexpr const char *inventory_name = "bundle-manifest.json";
static std::string read_text(const fs::path &path)
{
    require(fs::file_size(path) <= 64ULL * 1024 * 1024, "inventory too large");
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "cannot read inventory: " + path.u8string());
    std::ostringstream text;
    text << input.rdbuf();
    require(!input.bad(), "inventory read failed");
    return text.str();
}
std::string sha256(const fs::path &file)
{
    std::ifstream input(file, std::ios::binary);
    require(bool(input), "cannot hash file: " + file.u8string());
    Sha256 hash;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        hash.update(buffer.data(), static_cast<size_t>(input.gcount()));
    }
    require(input.eof() && !input.bad(), "file hash read failed: " + file.u8string());
    return hash.finish();
}
static std::string key(const std::string &name)
{
    if (name.empty())
        return {};
    (void)decode_utf8(name);
#ifdef _WIN32
    const auto wide = fs::u8path(name).wstring();
    const int count =
        LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr, 0);
    require(count > 0, "Unicode filename comparison failed");
    std::wstring upper(static_cast<size_t>(count), L'\0');
    require(LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, wide.data(), static_cast<int>(wide.size()), upper.data(), count, nullptr, nullptr,
                          0) == count,
            "Unicode filename comparison failed");
    return fs::path(upper).u8string();
#else
    static const bool unicode = [] {
        if (!std::setlocale(LC_CTYPE, "C.UTF-8"))
            std::setlocale(LC_CTYPE, "");
        return std::towupper(0x430) == 0x410;
    }();
    std::string result;
    for (const auto cp : decode_utf8(name)) {
        require(cp < 128 || unicode, "a UTF-8 locale is required for non-ASCII bundle destinations");
        append_utf8(result, static_cast<uint32_t>(std::towupper(static_cast<wint_t>(cp))));
    }
    return result;
#endif
}
static std::string relative_name(const std::string &value)
{
    require(!value.empty() && value.front() != '/' && value.back() != '/' && key(value.substr(0, value.find('/'))) != key(inventory_name),
            "invalid bundle destination: " + value);
    size_t begin = 0;
    while (begin < value.size()) {
        const auto end = value.find('/', begin);
        const auto part = value.substr(begin, end == std::string::npos ? end : end - begin);
        require(!part.empty() && part != "." && part != ".." && part.back() != '.' && part.back() != ' ', "invalid bundle path component: " + value);
        for (const unsigned char c : part)
            require(c >= 32 && c != 127 && std::string("\\:<>\"|?*").find(char(c)) == std::string::npos, "invalid Windows bundle name: " + value);
        auto basename = part.substr(0, part.find('.'));
        while (!basename.empty() && (basename.back() == ' ' || basename.back() == '.'))
            basename.pop_back();
        const auto device = key(basename);
        const auto points = decode_utf8(device);
        const bool numbered =
            device.size() == 4 && (device.compare(0, 3, "COM") == 0 || device.compare(0, 3, "LPT") == 0) && device[3] >= '0' && device[3] <= '9';
        const bool superscript = points.size() == 4 && (device.compare(0, 3, "COM") == 0 || device.compare(0, 3, "LPT") == 0) &&
                                 (points.back() == 0xb9 || points.back() == 0xb2 || points.back() == 0xb3);
        require(device != "CON" && device != "PRN" && device != "AUX" && device != "NUL" && device != "CONIN$" && device != "CONOUT$" && !numbered &&
                    !superscript,
                "reserved Windows device destination: " + value);
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return value;
}
struct Record {
    std::string name, hash;
    uint64_t size = 0;
};
struct Inventory {
    std::string component;
    std::vector<Record> records;
};
static Inventory inventory(const fs::path &root)
{
    const auto metadata = root / inventory_name;
    require(!fs::is_symlink(fs::symlink_status(metadata)) && fs::is_regular_file(metadata), "not an owned bundle directory: " + root.u8string());
    const auto data = read_text(metadata);
    const auto json = JsonReader(data).parse();
    require(json.at("format").integer() == 1 && json.at("files").kind == Json::Kind::Array, "unsupported bundle inventory");
    Inventory result;
    result.component = json.at("component").string();
    require(!result.component.empty(), "empty bundle owner");
    std::map<std::string, size_t> names;
    for (const auto &file : json.at("files").array) {
        Record record{relative_name(file.at("path").string()), file.at("sha256").string(), file.at("size").integer()};
        require(record.hash.size() == 64 && record.hash.find_first_not_of("0123456789abcdef") == std::string::npos, "invalid inventory digest");
        require(names.emplace(key(record.name), result.records.size()).second, "duplicate inventory path: " + record.name);
        result.records.push_back(std::move(record));
    }
    std::set<std::string> found;
    for (const auto &file : fs::recursive_directory_iterator(root)) {
        require(!file.is_symlink(), "symlink inside bundle: " + file.path().u8string());
        if (file.is_directory())
            continue;
        require(file.is_regular_file(), "special file inside bundle");
        if (file.path() == metadata)
            continue;
        const auto name = file.path().lexically_relative(root).generic_u8string();
        const auto match = names.find(key(name));
        require(match != names.end(), "unowned file inside bundle: " + name);
        const auto &record = result.records.at(match->second);
        require(record.name == name && record.size == file.file_size() && record.hash == sha256(file.path()), "edited bundle file: " + name);
        found.insert(match->first);
    }
    require(found.size() == result.records.size(), "bundle files missing");
    return result;
}
static bool within(const fs::path &path, const fs::path &root)
{
    auto a = path.begin(), b = root.begin();
    for (; b != root.end(); ++b, ++a)
        if (a == path.end())
            return false;
#ifdef _WIN32
        else if (key(a->u8string()) != key(b->u8string()))
            return false;
#else
        else if (*a != *b)
            return false;
#endif
    return true;
}
static std::string manifest(const Inventory &info)
{
    std::ostringstream out;
    out << "{\n  \"component\": " << quote(info.component) << ",\n  \"files\": [\n";
    for (size_t i = 0; i < info.records.size(); ++i) {
        const auto &record = info.records[i];
        out << "    {\n      \"path\": " << quote(record.name) << ",\n      \"sha256\": " << quote(record.hash)
            << ",\n      \"size\": " << record.size << "\n    }" << (i + 1 == info.records.size() ? "\n" : ",\n");
    }
    out << "  ],\n  \"format\": 1\n}\n";
    return out.str();
}
static fs::path unique_directory(const fs::path &parent)
{
    const auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    for (unsigned i = 0; i < 1024; ++i) {
        const auto path = parent / (".kw-stage-" + std::to_string(seed) + "-" + std::to_string(i));
        std::error_code error;
        if (fs::create_directory(path, error))
            return path;
        require(!error || error == std::errc::file_exists, "cannot create private staging directory");
    }
    throw std::runtime_error("cannot allocate private staging directory");
}
// Recursive cleanup is restricted to this newly-created, exclusive scratch,
// never to an existing bundle or an operator-selected directory.
struct Scratch {
    fs::path path;
    ~Scratch()
    {
        if (!path.empty()) {
            std::error_code error;
            fs::remove_all(path, error);
        }
    }
};
static void remove_owned_backup(const fs::path &backup, const Inventory &old, bool was_empty)
{
    if (!was_empty) {
        const auto verified = inventory(backup);
        require(verified.component == old.component, "backup owner changed");
        for (const auto &record : verified.records)
            fs::remove(backup / fs::u8path(record.name));
        fs::remove(backup / inventory_name);
    }
    std::vector<fs::path> directories;
    for (const auto &entry : fs::recursive_directory_iterator(backup))
        if (entry.is_directory() && !entry.is_symlink())
            directories.push_back(entry.path());
    std::sort(directories.begin(), directories.end(), [](const auto &a, const auto &b) { return a.native().size() > b.native().size(); });
    for (const auto &directory : directories)
        if (fs::is_empty(directory))
            fs::remove(directory);
    if (fs::is_empty(backup))
        fs::remove(backup);
}
void stage(const std::vector<std::string> &arguments)
{
    fs::path requested;
    std::string component;
    std::vector<std::string> files, overlays;
    for (size_t i = 0; i < arguments.size(); ++i) {
        const auto &option = arguments[i];
        require(i + 1 < arguments.size(), "missing argument for " + option);
        const auto &value = arguments[++i];
        if (option == "--output")
            requested = fs::u8path(value);
        else if (option == "--component")
            component = value;
        else if (option == "--file")
            files.push_back(value);
        else if (option == "--overlay")
            overlays.push_back(value);
        else
            throw std::runtime_error("unknown staging option: " + option);
    }
    require(!requested.empty() && !component.empty(), "--output and --component are required");
    (void)decode_utf8(component);
    for (unsigned char c : component)
        require(c >= 32 && c != 127, "invalid component name");
    const auto output = fs::absolute(requested).lexically_normal();
    require(output.has_filename() && output != output.root_path() && !fs::is_symlink(fs::symlink_status(output)) &&
                fs::weakly_canonical(output) == output,
            "bundle output must be a canonical, non-symlink path");
    struct Input {
        fs::path source;
        std::string name, expected_hash;
    };
    std::map<std::string, Input> entries;
    const auto add = [&](const fs::path &source, const std::string &destination, const std::string &expected) {
        const auto name = relative_name(destination);
        const auto resolved = fs::canonical(source);
        require(fs::is_regular_file(resolved) && !within(resolved, output), "invalid or overlapping input: " + source.u8string());
        require(entries.emplace(key(name), Input{resolved, name, expected}).second, "bundle destination collision: " + name);
    };
    for (const auto &value : files) {
        const auto separator = value.rfind('=');
        require(separator != std::string::npos, "expected SOURCE=DEST");
        add(fs::u8path(value.substr(0, separator)), value.substr(separator + 1), "");
    }
    for (const auto &name : overlays) {
        const auto overlay = fs::canonical(fs::u8path(name));
        const auto info = inventory(overlay);
        for (const auto &record : info.records)
            add(overlay / fs::u8path(record.name), record.name, record.hash);
    }
    require(!entries.empty(), "refusing an empty bundle");
    std::map<std::string, std::string> directory_spellings;
    for (const auto &[name, input] : entries) {
        for (size_t end = name.find('/'); end != std::string::npos; end = name.find('/', end + 1))
            require(!entries.count(name.substr(0, end)), "bundle directory/file prefix collision: " + name);
        for (size_t end = input.name.find('/'); end != std::string::npos; end = input.name.find('/', end + 1)) {
            const auto spelling = input.name.substr(0, end);
            const auto prior = directory_spellings.emplace(key(spelling), spelling);
            require(prior.second || prior.first->second == spelling, "bundle directory case alias collision: " + spelling);
        }
    }
    Inventory old;
    bool existed = fs::exists(output), was_empty = false;
    if (existed) {
        require(fs::is_directory(output), "output is not a directory");
        was_empty = fs::is_empty(output);
        if (!was_empty) {
            old = inventory(output);
            require(old.component == component, "bundle belongs to a different component");
        }
    }
    fs::create_directories(output.parent_path());
    const auto lock = output.parent_path() / ("." + output.filename().u8string() + ".kw-stage-lock");
    require(fs::create_directory(lock), "another bundle writer or a stale staging lock exists: " + lock.u8string());
    Scratch lock_guard{lock}, scratch{unique_directory(output.parent_path())};
    Inventory current;
    current.component = component;
    std::vector<Input> ordered;
    for (const auto &[name, input] : entries) {
        (void)name;
        ordered.push_back(input);
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
    for (const auto &input : ordered) {
        const auto target = scratch.path / fs::u8path(input.name);
        fs::create_directories(target.parent_path());
        require(fs::copy_file(input.source, target), "could not copy bundle input");
        const auto hash = sha256(target);
        require(input.expected_hash.empty() || input.expected_hash == hash, "overlay changed while staging");
        current.records.push_back({input.name, hash, fs::file_size(target)});
    }
    {
        std::ofstream metadata(scratch.path / inventory_name, std::ios::binary);
        metadata << manifest(current);
        metadata.close();
        require(bool(metadata), "cannot publish inventory in scratch directory");
    }
    // Recheck the destination immediately before replacing it. Failed validation
    // or copying above leaves its bytes untouched.
    require(fs::exists(output) == existed, "output changed while staging");
    if (existed) {
        require(fs::weakly_canonical(output) == output && !fs::is_symlink(fs::symlink_status(output)), "output changed while staging");
        if (was_empty)
            require(fs::is_empty(output), "empty output changed while staging");
        else
            require(manifest(inventory(output)) == manifest(old), "previous bundle changed while staging");
    }
    fs::path backup;
    if (existed) {
        backup = unique_directory(output.parent_path());
        fs::remove(backup);
        fs::rename(output, backup);
    }
    try {
        fs::rename(scratch.path, output);
        scratch.path.clear();
    } catch (...) {
        if (existed)
            fs::rename(backup, output);
        throw;
    }
    if (existed) {
        try {
            remove_owned_backup(backup, old, was_empty);
        } catch (const std::exception &error) {
            std::cerr << "Previous owned bundle retained at " << backup.u8string() << ": " << error.what() << '\n';
        }
    }
    std::cout << "STAGED " << component << ": " << current.records.size() << " files -> " << output.u8string() << '\n';
}
} // namespace kwmod_bundle
#ifndef KWMOD_STAGE_NO_MAIN
static int run_stage(const std::vector<std::string> &arguments)
{
    try {
        if (arguments.size() == 1 && (arguments[0] == "--help" || arguments[0] == "-h")) {
            std::cout << "kw_stage_bundle --output DIRECTORY --component NAME [--file SOURCE=DEST] [--overlay DIRECTORY]\n";
            return 0;
        }
        kwmod_bundle::stage(arguments);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Bundle refused: " << error.what() << '\n';
        return 1;
    }
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv)
{
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
        arguments.push_back(std::filesystem::path(argv[i]).u8string());
    return run_stage(arguments);
}
#else
int main(int argc, char **argv)
{
    return run_stage(std::vector<std::string>(argv + 1, argv + argc));
}
#endif
#endif
