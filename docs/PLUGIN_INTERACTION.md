# Independent plugins and optional cooperation

Use matching provider headers and runtime implementations. Optional integrations
must validate service versions and have a defined unavailable-provider path.

The mandatory dependency of a standalone plugin is `KWMOD_SDK_ROOT` only.
The loader SDK contains generic plugin/service/patch APIs and neutral KW engine
discovery, bounded reads, native hook context and schema-neutral configuration
parsing. It does not define pacing, AI, input-tap or presentation-worker protocols.

## Neutral loader API

`kwf/plugin.h` provides `publish(owner,id,major,api)` and
`service(owner,id,major)`. Providers publish during prepare; consumers check
NULL, the major version, structure size, required function pointers and any
provider-specific version fields. Providers retain the API and strings until
process exit. No plugin unload or replacement is introduced. Failed providers'
services are hidden; consumers must reacquire before relying on a provider that
may have failed activation. Prepare-phase service calls must remain read-only.

The loader owns the reserved `kwf.` service namespace. Plugins cannot publish
into it. New built-in `kwf.patch-query`, major 1 (`kwf/patch_query.h`) provides:

- `find`: one overlapping reservation's address, length, written flag,
  provider ID and preparation/activation state. No reservation is UNSUPPORTED;
  multiple overlaps or an insufficiently covered original read are conflicts.
- `original`: copies bounded original bytes retained by the broker, never a
  foreign plugin's arbitrary memory. It does not normalize an unregistered hook.

Both operations require a real plugin owner and the startup/engine owner thread.
They do not grant write rights or a way to bypass reserve/write/restore checks.
The existing host table and ABI 1.0 layout are unchanged; an older runtime
without this service is explicitly rejected where the query is required.
State values are 0 unavailable/failed, 1 prepared, 2 active. An owner may inspect
its own reservation while still preparing. An unbrokered patch remains unknown.

## Provider-owned protocols

| Provider | Public contract | Optional consumer flag |
| --- | --- | --- |
| Pacing | `kwpacing/integration/pacing_policy.h` | FPS: `WITH_PACING_INTEGRATION=1` |
| Threading | `kwthreading/integration/presentation_worker.h` | UI: `WITH_THREADING_INTEGRATION=1` |

Ordinary standalone builds keep these flags at 0. FPS and UI `bundle`/`stage`
builds automatically enable their cooperation code when the provider's public
headers are available. The workspace composition additionally restricts that
discovery to `BUNDLE_PLUGINS`, passed as `KWMOD_BUNDLE_PROVIDERS`: its default
composition enables FPS/pacing cooperation, but not UI/threading cooperation,
because experimental threading is excluded. An explicitly empty provider list
disables automatic selection. A separately built overlay has no composition
list and may compile cooperation with any available provider header.

Explicit `WITH_PACING_INTEGRATION=0|1` and `WITH_THREADING_INTEGRATION=0|1`
values, from the environment or Make arguments, override automatic selection.
Missing optional headers default to standalone behavior; an explicit 1 still
requires them and fails clearly when they are missing. Diagnostic input-flow
and timing probes are not release plugin interfaces or build targets.

A disabled integration adds no provider include path, source, library, DLL
import or required manifest dependency. Enabling one needs only that provider's
public headers, selected through `PACING_SDK_ROOT` or
`THREADING_SDK_ROOT` (sibling defaults). It does not link a copy of the provider,
stage it, enable its manifest or force it to be installed. Optional `after=`
relationships order existing providers without requiring absent plugins.

```sh
# Standalone FPS: loader SDK only.
make -C kw_plugin_fps_patch KWMOD_SDK_ROOT=/sdk/kw_modloader
# Explicit cooperation with installed pacing when available.
make -C kw_plugin_fps_patch WITH_PACING_INTEGRATION=1 PACING_SDK_ROOT=/src/kw_plugin_pacing
# Automatic composition-aware cooperation; threading/probe remain excluded.
make -C repos bundle
# Explicit standalone overlay even when pacing headers are present.
make -C kw_plugin_fps_patch bundle WITH_PACING_INTEGRATION=0
```

Settings stamps force recompilation/relinking when integration flags or provider
header locations change. Cross-plugin includes must be guarded by their explicit
integration flag and use only the provider's public headers. Disabled
integrations add no provider dependency.

## Runtime failure and safety

Missing optional providers are normal. A feature that requires a provider
rejects that requested feature rather than fabricating a callback. Consumers
must not identify an unknown foreign plugin, follow its jump or consume an
unregistered trampoline as an inferred cooperation contract.

FPS always checks neutral reservation metadata around the separately owned native
Sleep call. Optional protocols add pacing semantics/validation; disabling them
does not disable broker checks. Invalid sizes/versions, ambiguous ownership,
duplicate pacing providers or conflicting writes fail closed. Native wait
pacing has one implementation in `kw_pacing` for every role. The AI entry/game
DLL, controller and injector implement no pacing policy or native Sleep hook.

## Universal pacing and managed bot startup

Pacing has `roles=any` and depends only on the loader SDK. Human startup uses
the normal DirectInput factory; the bot injector loads that same bundle's
`dinput8.dll` and calls the generic `KwFrameworkStartInjected` entry before
initializing the bot game DLL or resuming the primary game thread. This adapter
has the Windows thread-entry calling convention and takes a numeric role.
It delegates to the existing `KwFrameworkStart`; no pacing-specific loader API
or AI/pacing dependency is introduced. The public entry is in `kwf/loader.h`.

Managed bots use `KWF_ROLE_HEADLESS`, including opt-in rendered bots. This role
selects bot-safe plugins, not the actual D3D state, and prevents recursive human
controller startup. Later DirectInput calls preserve the selected role and
continue forwarding to system DirectInput. Startup is once-only per process;
an attempted role change is rejected. Existing manifest values remain
`rendered`, `headless`, and `any`; there is no new manifest role or packet field.

The injector resolves the loader two directories above its grouped game DLL,
or accepts an explicit `--modloader DLL` for manually relocated experiments.
The standard composed layout needs no additional argument. Missing/incompatible
loader startup fails before the bot is resumed, never by using an AI-owned wait
hook as fallback. An absent/disabled pacing plugin is allowed: the bot then uses
native waits. Per-role pacing overrides remain solely in `kw_pacing.ini`.
Human runtime logs retain `kw_framework.log`; managed processes use
`kw_framework-bot-<pid>.log` to avoid sharing/truncating the human log.

Threading's presentation-worker exchange also moved out of the SDK. Its service
is now `kwthreading.presentation.worker`, not a reserved `kwf.` ID. UI compiles
that contract only with its optional integration enabled and otherwise uses the
serial FXAA path. Missing providers retain serial rendering. Threading remains
disabled/rejected by default; this is not new renderer-offload acceptance.
Do not mix old/new UI/threading binaries across this protocol relocation.

The legacy AI bridge still has other historical writers not all migrated to the
broker. This change does not claim universal safety for arbitrary third-party
patches or turn unknown hooks into trusted originals. Preserve the native byte
validation in every hook installer.

## Feature configuration and helpers

UI and gameplay have separate public config structures and parsers.
`kwengine/config.h` defines parsing primitives/descriptors only, and
`libkw_config.a` contains no feature names, controller endpoint, AI schema or
feature defaults. Dedicated feature files reject unknown/duplicate keys. Root
`kwbot_host.ini` fallback reads only recognized feature settings and ignores
other components' keys. Invalid dedicated files do not silently fall back.
Absent files use that plugin's defaults.

UI defaults/ranges and gameplay fields are checked against the old compatibility
parser using the current distributed INIs. The legacy full schema remains owned
by AI solely for compatibility; feature plugins do not link or include it.
Gameplay publishes no diagnostic input-flow service. Its release configuration
accepts only functional preservation settings; obsolete diagnostic keys in a
dedicated gameplay INI must be removed instead of silently activating old hooks.

Shared `kwengine/hook_context.h` provides FPU/MXCSR/LastError preservation for
UI, profiling and rendering hooks. Guards that explicitly
reset the FP environment or have different contracts remain feature-owned.
Other feature helpers and exported configuration APIs live in their
own plugin headers. UI/gameplay/threading private hook directories are now
`src/hooks/`; AI `src/game_interface/` implements its model/game-engine bridge.

`kwengine/config_values.h` provides shared UTF-8, scalar, RGB, trimming and bounded
string-copy primitives. The AI compatibility parser delegates to these same
implementations rather than retaining a second copy. `fixed100` parsing is
range-neutral; UI-specific bounds stay in the feature schema or legacy wrapper.

### Hook metadata capacity

The runtime registry grows during serialized startup preparation; plugins do
not share a fixed 256-record budget. Growth preserves owner/span conflicts,
idempotent reservations and original/replacement bytes. Allocation failure
leaves existing reservations intact. This changes no public plugin ABI.

Internal `kwf_patch_find` pointers are borrowed and must not survive registry
growth, owner rollback/compaction or disposal. The runtime finishes preparation
before activation; activation does not grow the table, and public broker calls
copy metadata instead of exposing these internal pointers. Patch spans remain
bounded by the existing per-record byte limit. Active records stay resident
for process lifetime.
