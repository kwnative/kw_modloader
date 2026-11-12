# KW-specific support SDK

The `kw_modloader` repository owns this game-specific implementation alongside
the generic loader/runtime. It does not require an AI plugin.
Shared public headers use `kwengine/`, not the AI plugin's `kwaibot/` namespace.
The C API uses `kwengine_*` / `KWENGINE_*` names. Build consumers against the
matching SDK version.
The generic loader implementation does not include game-specific headers.

## Artifacts

- `build/bin/kw_engine.dll` and `build/lib/libkw_engine.a`: the single process-wide
  original binding snapshot and 12 C exports declared by `src/engine/engine.def`.
  Include `<kwengine/engine_address.h>`. Link the import library, or explicitly load
  the DLL from the runtime root and bind the required exports. The first-party
  `include/kwengine/detail/engine_client.inc` is a private compatibility helper.
- `build/lib/libkw_timing.a`: FPS semantic discovery and path helpers from
  `src/timing`. Public types are under `include/kwengine/timing`. Native wait
  pacing uses `kw_engine.dll` directly. See `notices/fps_reference.txt` and
  [third-party notices](THIRD_PARTY_NOTICES.md).
- `build/lib/libkw_config.a`: schema-neutral bounded configuration parsing.
  Field descriptors, names, defaults and feature structures belong to plugins.
  The old full host parser is AI-owned, not part of this SDK.

## Discovery and mutation

Discover before mutation. The DLL preserves the original strict resolution
result and refuses a mismatched image. Consumers must not embed competing
resolver caches or reinterpret a foreign hook as a native address. Missing,
ambiguous or changed live bytes fail closed; there is no SHA/fixed-offset gate.
The original ready/code/data/span/patch-site checks remain required at hook
sites. Game-specific discovery alone grants no permission to write: plugins
still reserve and mutate through the modloader's patch broker.

The timing scanner retains its original native relationship/uniqueness checks.
It does not change native simulation time, packet serialization or game queues.
Public headers expose contracts/data, not permission for worker threads to call
arbitrary engine functions.

### Resolved prefix lookup

Successful discovery builds a private index of prefix-bearing symbols ordered
by resolved address. Normalization and patch-prefix validation use that index
instead of scanning unrelated symbols on every call. Duplicate addresses retain
original symbol order and prefix-length/mask rules. Reset and incomplete
resolution keep public checks closed. The index contains addresses and symbol
indices only: current bytes are still checked on every request. It is not a
code-byte cache, patch permission, or a replacement for complete instruction/ABI
validation. Out-of-image addresses are rejected before executable-section lookup.

`make test-prefix-index` checks both operations against independent linear
reference implementations using synthetic non-executable mapped data. It covers
aliases, masked operands, changed live bytes, unsupported lengths, outside-image
addresses and reset/not-ready states. No private game file is needed. Its timing
output measures only these lookup routines, not overall game performance.

## Beacon notification binding

`<kwengine/beacon_sources.h>` returns the checked native post-permission beacon
notification entry and its exact caller return address. The shared resolver
finds the unique callsite and follows its relative target; the helper validates
the call/target relationship, complete six-byte entry and cdecl return. Missing,
changed or incomplete resolution zeroes output and denies installation. It adds
no DLL exports, plugin protocol, callbacks or policy dependency. Consumers must
still prove active recipient role, native sender/object identity and thread/
episode coherence; the address itself grants no observation or lifecycle rights.

## Initial-map and cached-image sources

`<kwengine/map_sources.h>` groups resolved source locations for the authored
map holder, native map/ghost RTTI and eight resource-property NameKey wrappers.
`kwengine_map_sources` uses the existing process-wide resolver and validates
data spans; missing/incomplete resolution zeroes its output. It does not add
exports, hooks, engine calls, lazy-key initialization or an AI-plugin dependency.
The named-data resolver joins a unique engine string to its unique aligned
writable wrapper, independently of the current mutable NameKey value. It does
not substitute an asset-name hash or a reference-address delta.

These addresses remain private. A type, static location, cached image or readable
page does not authorize revealing current owner, health, occupancy or resources.
Consumers must establish source layout, owner-thread coherence, map/player/
episode identity and their own disclosure rules. Native cached-image allocations
can be reused; a pointer is not a stable public object identity. Native snapshot
callbacks mutate the engine and are not exposed as worker-thread queries.

`kwengine_object_fog_sources` additionally returns the checked Object secondary
shroud-interface vtable and its native hidden-when-fogged predicate. The binding
comes from Object RTTI with a secondary-this displacement of `0x64` and virtual
slot `0x14`, checked against the complete 33-byte entry prefix. This is **not**
the same slot in Object's primary interface. The helper calls no native function
and publishes no object data. Changed slot targets, code or data authority fail
closed with zero output.

The x86 predicate takes `this = Object + 0x64` and a player argument. Its result
means **hide in fog**, not disclose now. A missing partition also returns false;
neutrality, immobility, prior player discovery and native hide flags affect the
normal branches. Consequently it cannot authorize initial-map entries by
itself. An owner-thread producer still needs proven successful initial creation,
authored-property application and exact player/map/episode provenance. Do not
call it from a worker, use it to refresh hidden payload, or replace normal
shroud/stealth checks with it. Resolving this source neither installs a hook nor
completes an initial-public-map producer.

`kwengine_initial_map_hook_sources` validates the two full initial-creation
function entries and the authored-property handler entry. It also returns the
ordinary/bridge dictionary CALL return sites as provenance witnesses, verifying
their actual E8 targets. Return sites are **not** function entries and must not
be used for conventional trampolines. The helper neither installs hooks nor
decides which created objects may be published. A consumer still needs native
phase, partition, visibility, player and epoch checks. Partial or changed sources
produce zero output; no fixed-offset fallback is supplied.
The same binding includes a read-only native partition-registration witness.
It is not a fourth hook: it establishes the exact stock exclusion branch for
non-partitioned inert/projectile creations, never a public-visibility permission.

The explicit mapped-image fixture runs without executing game code:

```sh
make test-map-sources ENGINE_IMAGE=/path/to/compatible/cnc3ep1.dat
```

It tests only the new discovery sources plus one complete-cache refusal, rather
than rerunning all unrelated plugin installers for every mutation. No private
image is required by normal builds or bundled in the SDK. Windows GCC builds of
this fixture must link with `-Wl,--image-base,0x20000000` so the anonymous private
image can occupy its original address; the Make target sets this on native
Windows. This fixture never launches or changes the installed game.
Some 32-bit Wine layouts already occupy part of that synthetic range; exit77
then reports an unavailable fixture, not a resolver pass or game incompatibility.
The x86 native-callback and real SHM ABI fixtures are independent of this
non-executing mapped-image address requirement.

`<kwengine/replay_view_sources.h>` resolves the stock replay/observer viewed-
player setter and the ghost-manager virtual selection method. It checks both
native entry prefixes and the actual manager vtable slot; incomplete or changed
sources return zero output. Resolution itself calls no engine method and grants
no gameplay control. An offline consumer must resolve an original replay slot
by participant identity, prove owner thread/replay mode and bracket native calls
with `kwengine_hook_context`. Use the stock observer operation; never write
`PlayerList::local`, treat a replay slot as an engine index, or fabricate a live
selection authorization. The native ghost cache represents one selected local
viewpoint, not an array of complete independent player memories.

## Extension contracts

Provider-specific worker, pacing and input-tap protocols live in
their owning plugins, not this SDK. Consumers explicitly opt in at compile time.
The SDK supplies the registry and neutral `kwf.patch-query` API, not feature
payloads. See [plugin interaction](PLUGIN_INTERACTION.md) for exact contracts,
flags, runtime failure rules and the built-in read-only ownership query.

`kwengine/hook_context.h` provides neutral native FPU/MXCSR/LastError preservation;
`kwengine/native_status.h` retains the common native status/result layout without
AI launch/LAN structures or functions. Native status fields/layout and numeric
results remain unchanged, but their source names now reflect SDK ownership.

The full model-facing SHM/client API belongs to `kw_plugin_aibot/include`, not
this engine support SDK. Static design data, lawful live observations and current
action permission remain distinct domains. Fog, stealth and observer restrictions
must be enforced by the owning game interface.

## Build configuration

Set `KWMOD_SDK_ROOT` to the combined loader checkout; default is the sibling
`kw_modloader` directory. `BUILD_DIR` selects this SDK's output root. Consumers
use its `include/` tree and `KWMOD_BIN_DIR`/`KWMOD_LIB_DIR` for matching artifacts.
Compiler runtimes are statically linked; Windows system/CRT DLLs remain OS inputs.
No private game image is required to compile; fixture testing takes explicit
external paths and is not a release build dependency.
