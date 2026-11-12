# Build bundles, not live deployment

Human and managed bot processes use the same loader
and `kw_pacing` implementation. Bots bootstrap the bundle's loader before their
game interface; install a matching `dinput8.dll`, `kw_runtime.dll`, shared
`kw_engine.dll` and selected plugin manifests/DLLs in the composed layout.
Do not deploy only a new AI helper into an old detached runtime directory.
Pacing is optional; native waits remain when it is absent/disabled. AI contains
no pacing implementation. Use `plugins/kw_pacing.ini` for common defaults
and optional role overrides.
See the pacing README and `PLUGIN_INTERACTION.md` for exact startup semantics.

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
    kw_aibot.dll / kw_aibot.kwplugin / kw_aibot.ini
    kw_aibot/
      kw_aibot_game.dll
      kw_aibot_controller.exe
      kw_aibot_injector.exe
      kw_aibot_controller.ini.example
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
# UI + gameplay only, no AI:
make -C repos bundle BUNDLE_PLUGINS='kw_plugin_ui kw_plugin_gameplay'
# Choose the final composition output; source overlays retain separate outputs:
make -C repos bundle BUNDLE_DIR=/path/to/staging/bundle
```

Default composition includes UI, gameplay, pacing, FPS and AI. Threading,
harvester diagnostics and the example require explicit selection/build and
appropriate operator configuration; threading's manifest remains disabled.
Input-flow and logic-timing research probes are not release plugin targets or
interfaces. Default staging includes only each selected plugin's release files,
even when obsolete diagnostic outputs exist from an earlier developer build.

Bundle builds compile optional cooperation with the selected production plugins
automatically. In the default composition, FPS is built with
`WITH_PACING_INTEGRATION=1`; UI keeps `WITH_THREADING_INTEGRATION=0` because
threading is excluded. The composition passes its selected plugin list to
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

## AI configuration boundary

The entry DLL, manifest and configuration share the `kw_aibot` basename.
The host INI is in `plugins/`; it resolves the controller path from the engine/
loader root, not the working directory. Bridge loading resolves from the entry
DLL's absolute location into its `kw_aibot/` resource directory. The old
root `kwbot_host.ini` is a missing-file compatibility fallback only. A readable
invalid preferred INI fails rather than silently selecting the old file.

Only the controller configuration intentionally remains `.ini.example`; staging
does not promote its installation-specific paths/helper-hash placeholders to a
working user INI. Configure it and save as `kw_aibot_controller.ini` beside the
controller EXE. Paths inside that INI resolve from that same resource directory.
The standard template uses `..\..\cnc3ep1.dat`, four parent levels to the game
working directory/SkuDef, and five to the sibling writable `kwbot_state` root.
The installed engine, content, SkuDef, profiles and writable report directories
are installation inputs and are not shipped here. Human and bot roles reuse
the same installed engine and the root `kw_engine.dll`; do not provision a
second DAT or SDK copy in the plugin directory. The injector explicitly selects
the headless loader role before game entry, so the normal adjacent plugin tree
does not start a human bridge in the bot process. No global DLL search paths
are changed.

An adjacent Tacitus `dsound.dll` may itself import D3D9. The AI plugin's managed
headless path keeps that proxy loaded and redirects graphics factory calls to
its null backend in that bot process only. It does not edit Tacitus files,
network hooks, DLL overrides or the human process. This requires a matched AI
injector/game-interface pair; an older injector's module-presence guard can
reject the dependency even before rendering begins. Loaded D3D9 is not accepted
as evidence of headless operation: current factory-redirection attestation,
unchanged null hooks and the no-backend/window checks still apply. Internet
login/room integration remains a separate acceptance milestone.

The modular bridge requires the shared `kw_engine.dll` to be resolvable in
every bot process, as well as the human process. Review that runtime layout
when installing; the flat AI overlay alone is not a complete bot launch setup.
Install a complete matching loader/plugin composition, not a mixture of
unrelated versions. Preserve customized INIs during upgrades. The controller
fields `injector_executable`, `injector_sha256`, `game_interface_dll` and
`game_interface_sha256` identify its own matching helpers; they do not validate
game/SkuDef identity. No build target performs live installation changes.

No target installs into a game, overwrites a live user INI, starts Wine/the game,
or claims a new live-game compatibility pass. Before a public installer/archive
is released, its complete runtime composition still needs installation testing.
