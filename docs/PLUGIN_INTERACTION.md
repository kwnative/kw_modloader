# Independent plugins and optional cooperation

Use matching provider headers and runtime implementations. Optional integrations
must validate service versions and have a defined unavailable-provider path.

The mandatory dependency of a standalone plugin is `KWMOD_SDK_ROOT` only.
The loader SDK contains generic plugin/service/patch APIs and neutral KW engine
discovery, bounded reads, native hook context and schema-neutral configuration
parsing. Feature protocols belong to their owning plugins.

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
pacing is provided by `kw_pacing` for every role.

## Loader roles and universal pacing

Pacing has `roles=any` and depends only on the loader SDK. Normal startup
uses the DirectInput factory. An external process launcher can load the same
`dinput8.dll` and call `KwFrameworkStartInjected` before resuming the game.
This Windows thread-entry adapter takes a numeric role and delegates to
`KwFrameworkStart`; the public contract is in `kwf/loader.h`.

`KWF_ROLE_HEADLESS` selects the manifest role, independently of rendering state.
Later DirectInput calls preserve that role and forward to system DirectInput.
Startup is once-only per process; a role change is rejected. Manifest values
are `rendered`, `headless`, and `any`. Launchers own their startup ordering
and deployment policy. Absent or disabled pacing retains native waits;
per-role overrides belong solely to `kw_pacing.ini`.
Rendered logs use `kw_framework.log`; headless-role logs use
`kw_framework-bot-<pid>.log` to avoid collisions.

Threading's presentation-worker exchange also moved out of the SDK. Its service
is now `kwthreading.presentation.worker`, not a reserved `kwf.` ID. UI compiles
that contract only with its optional integration enabled and otherwise uses the
serial FXAA path. Missing providers retain serial rendering. Threading remains
disabled/rejected by default; this is not new renderer-offload acceptance.
Do not mix old/new UI/threading binaries across this protocol relocation.

The broker cannot guarantee safety for arbitrary third-party patches or turn
unknown hooks into trusted originals. Preserve the native byte
validation in every hook installer.

## Feature configuration and helpers

UI and gameplay have separate public config structures and parsers.
`kwengine/config.h` defines parsing primitives/descriptors only, and
`libkw_config.a` contains parsing primitives and no feature schemas or defaults.
UI reads only `plugins/kw_ui.ini`; gameplay reads only
`plugins/kw_gameplay.ini`. Dedicated files reject unknown/duplicate keys.
Missing files use the plugin defaults; invalid files fail instead of selecting
another component's configuration.

Gameplay publishes no diagnostic input-flow service. Its release configuration
accepts only functional preservation settings; obsolete diagnostic keys in a
dedicated gameplay INI must be removed instead of silently activating old hooks.

Shared `kwengine/hook_context.h` provides FPU/MXCSR/LastError preservation for
UI, profiling and rendering hooks. Guards that explicitly
reset the FP environment or have different contracts remain feature-owned.
Other feature helpers and exported configuration APIs live in their
own plugin headers. UI/gameplay/threading private hook directories are now
`src/hooks/`.

`kwengine/config_values.h` provides shared UTF-8, scalar, RGB, trimming and bounded
string-copy primitives. `fixed100` parsing is range-neutral; feature-specific
bounds stay in each plugin's schema.

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
