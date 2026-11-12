# Building and composing runtime bundles

Install a matching `dinput8.dll`, `kw_runtime.dll`, `kw_engine.dll` and selected
plugin overlays. Each plugin owns its configuration and feature-specific
deployment requirements. See the component README and `PLUGIN_INTERACTION.md`.

`make bundle` (alias `make stage`) builds and assembles `build/bundle/`.
Staging uses the native C++17 `kw_stage_bundle` utility, built automatically
with `HOST_CXX` (default `c++` on Linux, `WINDOWS_CXX` on Windows). It has no scripting-language or external JSON/
crypto-library dependency. Windows builds produce `kw_stage_bundle.exe` using
the native MinGW C++ compiler; the game DLLs still use `WINDOWS_CC/WINDOWS_CXX`.
The host utility lives under `build/host/`; `KWMOD_STAGE_BUILD_DIR` can relocate
that build separately from the game DLL output.
Override `BUNDLE_DIR=/absolute/staging/path` to choose another output directory.
The output is relative to the game engine directory, normally `RetailExe/1.2`:

```text
bundle/
  dinput8.dll                  # Loader bundle
  kw_runtime.dll
  kw_engine.dll
  README-bundle.md
  plugins/
    kw_ui.dll / kw_ui.kwplugin / kw_ui.ini
    ...                       # Other selected plugin overlays
  bundle-manifest.json
```

Each plugin's own target stages only its overlay (DLLs/EXEs, INIs and manifests).
The loader bundle supplies the shared runtime DLLs; it contains no plugins.
The example has its own `plugin_example/Makefile` and disabled manifest.
Build-only import/static libraries, objects and SDK headers are not bundled.
READMEs and available license notices are included under namespaced `docs/`
and `licenses/` directories, so independently owned overlays cannot collide.

The format-1 `bundle-manifest.json` inventory remains compatible with existing
bundles. The stager hashes files as streams, rejects edited/unowned outputs,
symlinks, invalid Windows destination names, case-insensitive collisions and
directory/file conflicts. UTF-8 source/output paths and Unicode inventory
strings are supported. On Linux a UTF-8 locale is needed for non-ASCII destination
case comparison. Copies finish in a private sibling directory before the owned
output is replaced. An output-specific `.kw-stage-lock` directory prevents two
new stagers from writing the same output at once. After an interrupted build,
remove a stale lock only after confirming its writer is no longer running.
An existing bundle is never an arbitrary recursive-cleanup target.

Run `make test-bundle test-bundle-integrations` for native ownership/inventory
checks and optional-provider Make-policy checks. Tests require the same Bash/GNU
Make build environment used by the recipes, and do not start the game or Wine.
`test-bundle` also runs `test-no-python` to prevent reintroducing that source dependency.

The workspace composition Makefile assembles loader plus selected overlays:

```sh
make -C repos bundle -j4
# Individual plugin overlay, with only one SDK dependency:
make -C repos/kw_plugin_ui bundle KWMOD_SDK_ROOT=/path/to/kw_modloader
# Select only UI and gameplay:
make -C repos bundle BUNDLE_PLUGINS='kw_plugin_ui kw_plugin_gameplay'
# Choose the final composition output; source overlays retain separate outputs:
make -C repos bundle BUNDLE_DIR=/path/to/staging/bundle
```

The composition repository selects the release profile; this loader repository
does not require or select any plugin. Experimental plugins need explicit
selection and appropriate operator configuration.
Input-flow and logic-timing research probes are not release plugin targets or
interfaces. Default staging includes only each selected plugin's release files,
even when obsolete diagnostic outputs exist from an earlier developer build.

Bundle builds compile optional cooperation with the selected production plugins
automatically. Selecting FPS and pacing enables
`WITH_PACING_INTEGRATION=1`; omitting threading keeps UI
`WITH_THREADING_INTEGRATION=0`. The composition passes its selected plugin list to
overlays as `KWMOD_BUNDLE_PROVIDERS`. Header detection never stages or enables
an experimental provider. Provider services are still optional at runtime;
absence retains the consumer's defined standalone behavior.

An individual FPS/UI overlay discovers available provider headers by default
for `bundle`/`stage`; standalone `make`/`make all` remains SDK-only. Explicit
`WITH_*_INTEGRATION=0|1` overrides either policy. Settings stamps rebuild objects
when the effective choice changes, so switching between standalone and bundle
targets does not reuse an incompatible object. See `PLUGIN_INTERACTION.md` for
provider header locations and failure/absence semantics. This compiles known
cooperation protocols, not universal compatibility with arbitrary third-party
patches; broker ownership and conflict checks remain mandatory.

Build-only policy and staging checks need no game or Wine:

```sh
make -C repos/kw_modloader test-bundle-integrations test-bundle
```

`bundle-manifest.json` lists exact relative filenames, sizes and SHA-256 hashes
of staged artifacts. This is a packaging inventory, **not a game/SkuDef hash
gate**. There are no timestamps or absolute source paths in it. Conflicting
overlay destinations, traversal, symlinks, unowned files and modified prior
outputs are rejected. Rerunning removes only obsolete files listed in the
previous verified inventory (for example, a deselected plugin), never recursively
cleans an arbitrary directory. Keep output directories dedicated to generated
bundles and unchanged. Make user edits after installation, not inside staging.
Archive the directory contents with your preferred ZIP/archive tooling.

## Manual installation

See [the Linux build and installation guide](BUILDING.md). Copy the loader bundle
to the engine directory and each selected plugin overlay onto it. Preserve
customized INIs on upgrades. Build/staging targets never install into a game.
