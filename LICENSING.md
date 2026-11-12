# Licensing and the proprietary-plugin boundary

Copyright (c) 2026 N.Leiten, for original project contributions and modifications.
Third-party copyrights and licenses remain with their respective holders.

## License map

| Material | License |
| --- | --- |
| `src/loader/`, `src/runtime/`, `src/core/` | GPL-3.0-only with the game and independent-plugin permissions below. |
| `src/engine/`, except `engine.def` | The same GPL terms and permissions; this is the runtime resolver implementation. |
| All `include/kwf/` and `include/kwengine/`, including inline code and `detail/*.inc` | MIT; see `SDK_LICENSE`. |
| `src/config/` and `src/timing/` | MIT reusable SDK implementation, with upstream attribution retained. |
| `src/engine/engine.def` and the import stubs generated from it | MIT. Import stubs do not contain the runtime resolver implementation. |
| `mk/` and `plugin_example/` | MIT, so an independent plugin can copy its build template and starter code. |
| Other original project files | GPL-3.0-only with the same additional permissions unless specifically marked otherwise. |

The complete GPL text is [LICENSE](LICENSE). The additional section-7 permissions
are [game interoperability](notices/GAME_EXCEPTION.txt) and
[independent plugin linking](notices/PLUGIN_EXCEPTION.txt). Both are applied to
the GPL-covered material identified above. Its machine-readable license identifier
is `LicenseRef-KW-Modloader`, with complete terms in
[LICENSES/LicenseRef-KW-Modloader.txt](LICENSES/LicenseRef-KW-Modloader.txt).
[REUSE.toml](REUSE.toml) records the per-path split. MIT material remains available
separately under [SDK_LICENSE](SDK_LICENSE); copying it into a GPL build does not
remove that separate permission.

## What a proprietary plugin may use

An independently written plugin may include the complete public header tree,
including header-only helper implementations, copy the minimal example and Make
templates, and statically link `libkw_config.a` and `libkw_timing.a`. It may use
the `libkw_engine.a` import stubs or bind the exported engine API dynamically.
The MIT attribution and permission notice must accompany copies or substantial
portions of that SDK material, including when incorporated into a binary.

The explicit independent-plugin permission allows the plugin to load, link and
communicate through the public `kwf`/`kwengine` interfaces with `dinput8.dll`,
`kw_runtime.dll` and `kw_engine.dll`, while keeping its own proprietary license
and source private. The permission is needed for the GPL runtime boundary;
permissive header declarations alone are not the whole licensing arrangement.

This does not permit taking `src/core/core.c`, `src/runtime/runtime.c`, the
resolver implementation or other covered implementation into a closed plugin.
Changes to those covered components remain GPL-covered. A private copy, wrapper,
renamed file or separate DLL does not change that requirement. The shipped
plugins have their own GPL terms; this exception does not relicense their code,
headers or provider-specific services.

## Distribution

Distributing a proprietary plugin alone does not require distributing the
loader's source solely because it uses the permitted interfaces. If you also
distribute GPL-covered loader/runtime binaries, comply with GPLv3 for those
components: include the GPL and permission notices and provide matching
Corresponding Source, including your changes and the build scripts. Merely
including these license files is not sufficient source compliance. Keep MIT and
upstream notices for the SDK. Example copies distributed separately must retain
their MIT notice.

Contributions to the GPL runtime should retain these permissions if proprietary
plugin interoperability is intended. GPLv3 section 7 allows downstream removal
of additional permissions, so check the actual fork's notices before relying
on them. Code contributed under incompatible or plain GPL terms must not silently
be treated as carrying this exception.

The game permission covers the relationship with the proprietary game on the
project copyright holders' side only. No permission to redistribute game files,
assets or third-party mods is granted. Their owners' terms remain separate.

## Third parties and references

Timing helpers adapted from `CNCStuff/cnc3_fps_patch` retain their original
WTFPL provenance; the project additions to the reusable SDK are MIT. See
[third-party notices](docs/THIRD_PARTY_NOTICES.md). Permissive upstream material
does not become exclusively owned by this project.

The public plugins are GPL projects, not proprietary-plugin examples. Use
`plugin_example/` as the MIT starting point for a closed-source plugin.
