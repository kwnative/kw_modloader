# Linux builds and manual installation

The Linux host builds Windows x86 DLLs for Kane's Wrath. A game installation is
not needed to compile. Wine is needed only to run the game or Windows test
executables. Building plugins requires this loader SDK; loading plugins requires
the matching loader runtime installed with the game.

## Prerequisites

- Bash and GNU Make 4.3 or newer.
- A native C/C++ toolchain supporting C++17 for bundle staging and native tests.
- MinGW-w64 targeting Windows **i686**: `i686-w64-mingw32-gcc`,
  `i686-w64-mingw32-g++` and `i686-w64-mingw32-ar`.

On Debian/Ubuntu, the package prerequisites are `build-essential mingw-w64`.
Other distributions should install equivalent packages. The build uses standard
shell utilities such as `find`, `cmp` and `mktemp`. Compiler runtimes are linked
statically; Windows system DLLs remain runtime dependencies.
Some optional 32-bit Linux fixtures additionally require multilib development
libraries and `qemu-i386`. These are not prerequisites for release DLL builds.

## Build the loader

From the loader repository:

```sh
make -j2
make bundle
```

`build/bin/` contains `dinput8.dll`, `kw_runtime.dll` and `kw_engine.dll`.
`build/lib/` contains import/static libraries used by plugins. `build/bundle/`
contains the installable layout, license notices, documentation and inventory.
It contains no plugins or game files.

Useful overrides, accepted as environment variables or Make arguments:

| Variable | Purpose |
| --- | --- |
| `WINDOWS_CC`, `WINDOWS_CXX`, `WINDOWS_AR` | Windows x86 cross-toolchain executables. |
| `HOST_CXX` | Native C++17 compiler for the staging utility. |
| `BUILD_DIR` | This component's generated build directory. |
| `BUNDLE_DIR` | Dedicated generated staging directory; never point it at the game. |

## Build an independent plugin

Build the loader first, then run this from the selected plugin checkout:

```sh
export KWMOD_SDK_ROOT=/path/to/kw_modloader
make -j2
make sdk-paths
make bundle
```

The sibling `../kw_modloader` is the default SDK location. Relocated checkouts
use the variable above. A plugin's `build/bundle/` is an overlay, not a complete
loader installation. Each plugin documents its optional provider contracts;
other plugins are not mandatory SDK dependencies.

For a loader built outside its default `build/`, point `KWMOD_BIN_DIR` and
`KWMOD_LIB_DIR` at matching `bin/` and `lib/` directories under that output root.
The plugin SDK helper builds missing/stale shared artifacts from the chosen
loader checkout. Do not mix headers and libraries from unrelated versions.

## Install manually

Close the game. The destination is the directory containing `cnc3ep1.dat`,
usually `<game>/RetailExe/1.2`, rather than the launcher directory.

1. Copy the contents of the loader's `build/bundle/` into that destination.
2. Copy the contents of each selected plugin's `build/bundle/` onto the same
   destination, preserving subdirectories. Keep license/documentation files.
3. On upgrades, preserve customized `.ini` files and merge newly introduced
   settings from the supplied templates.
4. Check the plugin manifest's `enabled` value. Experimental plugins can ship
   disabled. Restart the game after changing startup settings.

Example installed layout:

```text
RetailExe/1.2/
  cnc3ep1.dat                 # Existing game engine; not distributed here
  dinput8.dll
  kw_runtime.dll
  kw_engine.dll
  plugins/
    kw_ui.dll
    kw_ui.kwplugin
    kw_ui.ini
  docs/
  licenses/
```

Only this loader supplies `dinput8.dll`. Do not overwrite an unrelated proxy
without arranging its integration first. On Wine, configure `dinput8` as
native-first for this game if needed; a per-launch example from the game root is:

```sh
WINEDLLOVERRIDES='dinput8=n,b' wine cnc3ep1.exe
```

That environment setting selects the loader; it is not an installation path.
No `install` Make target or installation-directory variable currently exists.
Build and bundle targets do not modify an installed game. See
[bundle composition](BUNDLES.md) for packaging selected overlays together.

## Windows build hosts

Native Windows build instructions and qualification are planned. Keep build
scripts based on GNU Make/Bash and compiler overrides, with generated Windows
executables statically linked. The Linux instructions above do not claim a
tested native Windows build workflow.
