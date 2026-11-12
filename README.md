# kw_modloader

Kane's Wrath plugin loader, runtime and shared engine SDK in one repository.

The loader/runtime are **GPLv3**, with an explicit permission for independent
proprietary plugins. The public SDK, inline helpers, static config/timing helpers
and plugin example are **MIT**. See [the license map](LICENSING.md) before copying
implementation code or distributing binaries.

See [release licensing](docs/RELEASING.md) for matching-source distribution and
[contribution terms](CONTRIBUTING.md) for keeping the SDK/runtime boundary intact.

`KWMOD_SDK_ROOT` is the only SDK checkout plugins need. Generic contracts remain
under `include/kwf/`; all shared game-specific support is under `include/kwengine/`.
These are internal responsibility boundaries,
not separate repositories. The AI/model SHM interface stays with the AI plugin.

## Layout and build

Production implementation is under `src/`; supported/public or compatibility
headers are under `include/`. Generated files go only to ignored `build/`:
`build/bin` for DLLs/executables, `build/lib` for libraries, `build/obj` for
objects/dependency files, `build/config` for copied configuration templates.
Use GNU Make and x86 MinGW (`i686-w64-mingw32-gcc/g++/ar`).
Bundle staging additionally builds a native C++17 host utility with `HOST_CXX`.
See the bundle guide for host/output overrides and offline test targets.

```sh
make -j4
make sdk-paths
```

For relocated plugin checkouts set `KWMOD_SDK_ROOT=/path/to/kw_modloader`.
Environment variables and Make arguments are supported. SDK DLLs and libraries
are located through `KWMOD_BIN_DIR` and `KWMOD_LIB_DIR`. Header-only callbacks
do not require linking a private copy of `kw_runtime.dll`.

An SDK binary override must point to the matching SDK build (`bin`/`lib` below
one output root). `sdk-build` builds missing/stale shared artifacts from the
selected SDK checkout. Compiler paths can be overridden with `WINDOWS_CC`,
`WINDOWS_CXX`, `WINDOWS_AR`; `BUILD_DIR` overrides this repository's output.

`make` builds `dinput8.dll`, `kw_runtime.dll`, `kw_engine.dll`, its import library,
and shared timing/config static libraries. Engine support sources are under `src/engine`, `src/config`, and
`src/timing`; see [engine API](docs/ENGINE_API.md).

`make bundle` (alias `make stage`) assembles the three runtime DLLs and a README
into `build/bundle/`, with a hash inventory. Plugin repositories provide overlay
bundles with their own binaries, configs and manifests. The workspace composition
target combines them. See [bundle commands and configuration limits](docs/BUNDLES.md).

Build and staging targets do not install into a game or overwrite user settings.
Use matching loader/SDK artifacts and preserve customized INIs when installing.
Runtime loading uses `.kwplugin` manifests; configuration uses `.ini` files.
