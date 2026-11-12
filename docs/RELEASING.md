# Source and binary release licensing

The loader and shipped plugins are GPLv3 projects with explicit permissions;
the reusable loader SDK is MIT. See [LICENSING.md](../LICENSING.md) for the exact
boundary. These notes describe distribution, not installation into a game.

## Publishing a source repository

Keep `LICENSE`, `LICENSING.md`, `REUSE.toml`, `LICENSES/`, `notices/` and upstream
notices in the repository. Keep `SDK_LICENSE` and the minimal example's MIT
`LICENSE` in the loader repository. First-party notices do not overwrite
third-party copyright. For contributions to the GPL runtime, obtain permission
under the same GPL-plus-exceptions terms before including them in builds that
promise proprietary-plugin interoperability.

The custom machine-readable `LicenseRef-KW-Modloader` and
`LicenseRef-KW-Game-Plugin` mean the full GPLv3 text plus the additional
permissions, not a replacement proprietary license. Their complete texts are
in `LICENSES/`. MIT SDK file tags and the `REUSE.toml` overrides identify the
permissive subset, including inline and statically linked implementations.

## Distributing binaries

1. Record the exact source revision and build configuration for each component.
2. Provide the matching Corresponding Source through a GPLv3-compliant method.
   For downloads, provide access to that source in the same place in the manner
   allowed by GPLv3 section 6(d). A link to a moving branch is insufficient.
3. Include modifications, build scripts and source for linked non-system
   dependencies as required. For AI tools this includes the exact ggml checkout,
   `nlohmann/json` headers and vendored Zstandard used for the binaries. Record
   external revisions and local changes; they are not made available merely by
   publishing this repository, which does not vendor every dependency.
4. Include project licenses, exception texts, MIT SDK notice and dependency
   notices. `make bundle` stages them at component-specific paths under both
   `licenses/` and `docs/`. AI CMake installation also installs these notices.
5. Keep game executables, game assets, third-party mod content, replays and model
   weights out of source/binary packages unless separately authorized. These
   project licenses do not grant rights to those materials.

License staging does not produce or host Corresponding Source automatically.
Publishing a source-only repository and publishing a binary download are
different distribution steps. The GPL requirements continue to apply to loader
and runtime binaries even when packaged alongside a proprietary plugin.

## Independent proprietary plugin

Use the MIT public SDK and example; retain the MIT notice with your distribution.
The loader's independent-plugin permission allows the documented public API
interaction while leaving your independent code under your chosen license.
If you distribute only that plugin, the permission does not require its source.
If you also ship the loader/runtime, provide their source and notices as above.
Copying GPL runtime or another GPL plugin's implementation is outside that
permission. A downstream fork can remove section-7 permissions, so verify the
notices for the actual runtime you distribute.

## References

- [GNU GPLv3, especially sections 6 and 7](https://www.gnu.org/licenses/gpl-3.0.html)
- [GNU FAQ: GPL plugins](https://www.gnu.org/licenses/gpl-faq.html#GPLPlugins)
- [GNU FAQ: exceptions for incompatible libraries](https://www.gnu.org/licenses/gpl-faq.html#GPLIncompatibleLibs)
- [GNU license list: WTFPL compatibility](https://www.gnu.org/licenses/license-list.html#WTFPL)

The project-specific permissions are deliberately explicit. They should be
reviewed as legal terms before a public binary release; successful builds and
packaging checks do not establish legal enforceability or third-party rights.
