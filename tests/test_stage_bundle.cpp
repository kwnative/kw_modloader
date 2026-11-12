/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#include "../tools/bundle_stage.hpp"
#include "../tools/bundle_format.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using kwmod_bundle::require;
static std::string read(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "test read failed");
    return std::string(std::istreambuf_iterator<char>(input), {});
}
static void write(const fs::path &path, const std::string &value)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << value;
    require(bool(output), "test write failed");
}
static std::map<std::string, std::string> snapshot(const fs::path &root)
{
    std::map<std::string, std::string> result;
    for (const auto &item : fs::recursive_directory_iterator(root)) {
        const auto name = item.path().lexically_relative(root).generic_u8string();
        result[name] = item.is_symlink()     ? "link:" + fs::read_symlink(item.path()).generic_u8string()
                       : item.is_directory() ? "directory"
                                             : "file:" + read(item.path());
    }
    return result;
}
struct Fixture {
    fs::path root, source, output;
    Fixture()
    {
        const auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0; i < 1000; ++i) {
            root = fs::temp_directory_path() / ("kw-bundle-test-" + std::to_string(seed) + "-" + std::to_string(i));
            if (fs::create_directory(root))
                break;
            require(i != 999, "cannot allocate fixture");
        }
        source = root / "plugin.dll";
        output = root / "output";
        write(source, "abc");
    }
    ~Fixture()
    {
        std::error_code error;
        fs::remove_all(root, error);
    }
    std::vector<std::string> arguments(const std::string &name = "plugins/test.dll", const fs::path &destination = {}) const
    {
        return {"--output", (destination.empty() ? output : destination).u8string(), "--component", "test", "--file", source.u8string() + "=" + name};
    }
    void run(const std::string &name = "plugins/test.dll")
    {
        kwmod_bundle::stage(arguments(name));
    }
    void refuse(const std::vector<std::string> &args)
    {
        const auto before = snapshot(root);
        bool failed = false;
        try {
            kwmod_bundle::stage(args);
        } catch (const std::exception &) {
            failed = true;
        }
        require(failed, "invalid staging accepted");
        require(snapshot(root) == before, "refused staging changed existing files");
    }
};
int main()
{
    try {
        unsigned tests = 0, skipped = 0;
        const auto test = [&](const char *name, auto check) {
            Fixture fixture;
            const auto skipped_before = skipped;
            check(fixture);
            ++tests;
            std::cout << (skipped > skipped_before ? "SKIP " : "PASS ") << name << '\n';
        };
        test("streaming SHA256 standard vectors", [](Fixture &f) {
            require(kwmod_bundle::sha256(f.source) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc SHA mismatch");
            write(f.source, "");
            require(kwmod_bundle::sha256(f.source) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty SHA mismatch");
            write(f.source, std::string(1000000, 'a'));
            require(kwmod_bundle::sha256(f.source) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "stream SHA mismatch");
        });
        test("manifest compatibility and repeat", [](Fixture &f) {
            f.run();
            const auto before = read(f.output / "bundle-manifest.json");
            const auto info = kwmod_bundle::JsonReader(before).parse();
            require(info.at("format").integer() == 1 && info.at("files").array.size() == 1, "manifest shape differs");
            const auto &record = info.at("files").array[0];
            require(record.at("size").integer() == 3 && record.at("path").string() == "plugins/test.dll" &&
                        record.at("sha256").string() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                    "manifest record differs");
            f.run();
            require(read(f.output / "bundle-manifest.json") == before, "non-deterministic manifest");
        });
        test("old format-1 inventory accepted", [](Fixture &f) {
            write(f.output / "plugins/test.dll", "abc");
            write(f.output / "bundle-manifest.json",
                  "{\"format\":1,\"component\":\"test\",\"files\":[{\"path\":\"plugins/test.dll\","
                  "\"size\":3,\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\"}]}\n");
            f.run();
        });
        test("owned stale files removed", [](Fixture &f) {
            f.run();
            f.run("other.dll");
            require(!fs::exists(f.output / "plugins/test.dll") && read(f.output / "other.dll") == "abc", "stale payload remains");
        });
        test("unowned output refused", [](Fixture &f) {
            write(f.output / "user.ini", "preserve");
            f.refuse(f.arguments());
        });
        test("edited prior payload refused", [](Fixture &f) {
            f.run();
            write(f.output / "plugins/test.dll", "user edit");
            f.refuse(f.arguments());
        });
        test("extra file refused", [](Fixture &f) {
            f.run();
            write(f.output / "user.ini", "user");
            f.refuse(f.arguments());
        });
        test("missing prior file refused", [](Fixture &f) {
            f.run();
            fs::remove(f.output / "plugins/test.dll");
            f.refuse(f.arguments());
        });
        test("different owner refused", [](Fixture &f) {
            f.run();
            auto args = f.arguments();
            args.insert(args.end(), {"--component", "other"});
            f.refuse(args);
        });
        test("traversal aliases and device names refused", [](Fixture &f) {
            for (const auto *name : {"../escape.dll",
                                     "/absolute.dll",
                                     "C:/absolute.dll",
                                     "plugins\\escape.dll",
                                     "./foo.dll",
                                     "plugins//foo.dll",
                                     "plugins/",
                                     "bundle-manifest.json",
                                     "Bundle-Manifest.json",
                                     "bundle-manifest.json/child",
                                     "Bundle-Manifest.json/child",
                                     "NUL.dll",
                                     "a/CON",
                                     "CON .txt",
                                     "COM1.log",
                                     "LPT9.txt",
                                     "trailing.",
                                     "trailing ",
                                     "a?.dll",
                                     "COM¹.log",
                                     "LPT².txt"})
                f.refuse(f.arguments(name));
        });
        test("case-insensitive collision refused", [](Fixture &f) {
            auto args = f.arguments("Test.dll");
            args.insert(args.end(), {"--file", f.source.u8string() + "=test.dll"});
            f.refuse(args);
        });
        test("directory/file collision refused", [](Fixture &f) {
            auto args = f.arguments("plugins");
            args.insert(args.end(), {"--file", f.source.u8string() + "=plugins/test.dll"});
            f.refuse(args);
        });
        test("directory case alias collision refused", [](Fixture &f) {
            auto args = f.arguments("Directory/a.dll");
            args.insert(args.end(), {"--file", f.source.u8string() + "=directory/b.dll"});
            f.refuse(args);
        });
        test("self input refused", [](Fixture &f) { f.refuse(f.arguments("file.dll", f.root)); });
        test("source failure preserves previous output", [](Fixture &f) {
            f.run();
            auto args = f.arguments();
            args.insert(args.end(), {"--file", (f.root / "missing").u8string() + "=missing.dll"});
            f.refuse(args);
        });
        test("overlay and duplicate overlay", [](Fixture &f) {
            f.run();
            const auto combined = f.root / "combined";
            std::vector<std::string> args{"--output", combined.u8string(), "--component", "combined", "--overlay", f.output.u8string()};
            kwmod_bundle::stage(args);
            require(read(combined / "plugins/test.dll") == "abc", "overlay payload differs");
            args.insert(args.end(), {"--overlay", f.output.u8string()});
            f.refuse(args);
        });
        test("case-only rename", [](Fixture &f) {
            f.run("Test.dll");
            f.run("test.dll");
            const auto info = read(f.output / "bundle-manifest.json");
            require(info.find("test.dll") != std::string::npos && info.find("Test.dll") == std::string::npos, "rename inventory differs");
#ifndef _WIN32
            require(!fs::exists(f.output / "Test.dll"), "old case path remains");
#endif
        });
        test("UTF-8 input and destination paths", [](Fixture &f) {
            f.source = f.root / fs::u8path(u8"джерело.dll");
            write(f.source, "abc");
            const auto output = f.root / fs::u8path(u8"дані output");
            kwmod_bundle::stage(f.arguments(u8"plugins/плагін.dll", output));
            require(read(output / fs::u8path(u8"plugins/плагін.dll")) == "abc", "UTF-8 payload differs");
        });
        test("UTF-8 case collision", [](Fixture &f) {
            auto args = f.arguments(u8"ПЛАГІН.dll");
            args.insert(args.end(), {"--file", f.source.u8string() + u8"=плагін.dll"});
            f.refuse(args);
        });
        test("malformed inventory refused", [](Fixture &f) {
            f.run();
            write(f.output / "bundle-manifest.json", "{\"format\":1,\"format\":1,\"files\":[]}");
            f.refuse(f.arguments());
        });
        test("invalid JSON Unicode and integers refused", [](Fixture &) {
            for (const auto *text : {"\"\\ud800\"", "\"\\udc00\"", "18446744073709551616", "01", "{} trailing", "[1,]"}) {
                bool failed = false;
                try {
                    (void)kwmod_bundle::JsonReader(text).parse();
                } catch (const std::exception &) {
                    failed = true;
                }
                require(failed, "malformed JSON accepted");
            }
            const auto parsed = kwmod_bundle::JsonReader("\"\\ud83d\\ude80\"").parse();
            require(parsed.string() == u8"🚀", "valid surrogate pair decoded incorrectly");
        });
        test("empty and unknown options refused", [](Fixture &f) {
            f.refuse({"--output", f.output.u8string(), "--component", "test"});
            f.refuse({"--output", f.output.u8string(), "--unknown", "x"});
        });
        test("existing staging lock refused", [](Fixture &f) {
            fs::create_directory(f.root / ".output.kw-stage-lock");
            write(f.root / ".output.kw-stage-lock/owner", "another writer");
            f.refuse(f.arguments());
        });
        test("hidden destination and equals in source name", [](Fixture &f) {
            f.source = f.root / "plugin=name.dll";
            write(f.source, "abc");
            f.run("plugins/.hidden.dll");
            require(read(f.output / "plugins/.hidden.dll") == "abc", "hidden payload differs");
        });
        test("symlink output refused", [&](Fixture &f) {
            const auto actual = f.root / "actual";
            fs::create_directory(actual);
            std::error_code error;
            fs::create_directory_symlink(actual, f.output, error);
            if (error) {
                ++skipped;
                return;
            }
            f.refuse(f.arguments());
            require(fs::is_empty(actual), "symlink target modified");
        });
        test("symlink prior file refused", [&](Fixture &f) {
            f.run();
            fs::remove(f.output / "plugins/test.dll");
            std::error_code error;
            fs::create_symlink(f.source, f.output / "plugins/test.dll", error);
            if (error) {
                ++skipped;
                return;
            }
            f.refuse(f.arguments());
        });
        test("symlink parent output refused", [&](Fixture &f) {
            const auto actual = f.root / "actual", alias = f.root / "alias";
            fs::create_directory(actual);
            std::error_code error;
            fs::create_directory_symlink(actual, alias, error);
            if (error) {
                ++skipped;
                return;
            }
            f.refuse(f.arguments("plugins/test.dll", alias / "bundle"));
        });
        std::cout << "PASS bundle cases=" << tests << " symlink_cases_skipped=" << skipped << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL bundle: " << error.what() << '\n';
        return 1;
    }
}
