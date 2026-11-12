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
These are internal responsibility boundaries within the same repository.
Feature-specific interfaces and configuration belong to their owning plugins.

## Layout and build

The supported build instructions currently target a **Linux host**, producing
32-bit Windows DLLs. See [Linux build and manual installation](docs/BUILDING.md)
for prerequisites, compiler overrides and the installed layout.

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

## Manual installation

Run `make bundle`, close the game, and copy the contents of `build/bundle/` to
the directory containing `cnc3ep1.dat`, normally `RetailExe/1.2`. Keep
`dinput8.dll`, `kw_runtime.dll` and `kw_engine.dll` together. Copy each desired
plugin's bundle onto the same directory; plugins require this loader to run.
Preserve customized plugin INIs when upgrading. On Wine, select native-first
`dinput8` loading for this game if it is not already configured.

There is no `make install` target. `BUNDLE_DIR` selects a generated staging
directory, not an installation destination. Native Windows build instructions
remain future work; the compiler/output overrides are retained for that path.
