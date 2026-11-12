# Plugin SDK contract (ABI 1.0)

Include `<kwf/plugin.h>` from `include/`. The generic SDK is independent of game-specific features, graphics and networking. Runtime callbacks are supplied as a
function table, so a plugin does not need an import library for `kw_runtime.dll`.
The generic runtime artifacts are `build/bin/dinput8.dll` and `kw_runtime.dll`;
the same repository also builds shared `kw_engine.dll` and engine helper libraries.

## Entry and lifecycle

The only mandatory plugin export is `KwPluginQuery`, with C linkage and
`KWF_CALL` (`__cdecl` on Windows). The plugin DLL is Windows x86, regardless of
the bitness of an external client communicating with it. C++ can be used
internally, but classes/STL/exceptions/allocator ownership must not cross the ABI.

`KwPluginQuery` returns a process-lifetime `kwf_plugin` descriptor. The loader
validates struct size, ABI, role and required capabilities. `prepare(context)`
may read configuration, discover engine addresses, publish service tables and
reserve patch ranges, but cannot mutate game memory or start engine work.
All preparation completes before activation starts. `activate()` runs outside
DllMain, on the startup/engine-owner thread, and may install approved patches.
Query/DllMain must not hook, perform IO, load modules or spawn threads.

Roles are `rendered`, `headless` or both. Disabled/wrong-role plugins are filtered
before LoadLibrary. Normal DInput8 startup selects rendered; an explicitly audited
injector can use `KwFrameworkStart(role)`. Role cannot change after startup.
There is no dynamic unload callback: hooked modules stay resident until exit.

## Manifest

The loader reads only direct `plugins/*.kwplugin` entries relative to itself.
The mandatory keys are `id`, `library`, `abi`, `enabled`, `roles`; optional
`requires` and `after` are comma-separated plugin IDs. Use lowercase IDs and
DLL basenames, no path traversal. Unknown/duplicate keys or incompatible values
refuse that entry. `requires` is a hard dependency; `after` orders an available
provider without making it mandatory. Declare service-provider dependencies.
Cycles/failures refuse the affected component, not unrelated plugins or system
DirectInput forwarding. See `plugin_example/` for source and manifest.

## Host callbacks

- `log(owner,text)`: bounded runtime logging.
- `reserve(owner,address,length)`: prepare-time ownership claim. Current core
  allows 256 ranges, each at most 64 bytes. This is not a native game queue limit.
- `write(owner,address,expected,replacement,length)`: owner-thread activation/
  later write, requiring the exact reserved range and original/expected bytes.
- `restore(owner,address,length)`: restore only if current bytes still match
  that owner's replacement; never overwrite an unrelated later mutation.
- `publish(owner,id,major,api)`: prepare-time publication of a static resident
  service table. Current runtime permits 32 services and rejects duplicate ID/major.
  The `kwf.` prefix is reserved for loader-owned APIs and cannot be published by plugins.
- `service(owner,id,major)`: NULL if unavailable. Consumers validate service
  structure size/version and readiness; publication is not activation success.

Failed activation rolls back matching owned writes in reverse order. No detour
chaining is implied: overlapping writers need an audited shared dispatcher or
must be refused. Plugins are trusted native code, not a memory-security sandbox.

## Extending APIs

A provider distributes its small public C header with a named/versioned table.
Use fixed-width fields, explicit sizes/versions, `KWF_CALL`, documented threading,
readiness, borrowed-buffer lifetimes and status codes. Do not publish stack or
temporary tables, silently replace another provider, or free memory with a
different module's allocator. Incompatible changes need a new service major;
append-only optional fields still require size checks. A missing optional service
needs a defined serial/disabled fallback.

Shared KW functions live under `kwengine/` in this same SDK repository. Provider
protocols live in the owning plugins behind optional consumer build flags, not
in the SDK. The built-in neutral read-only patch query is in `kwf/patch_query.h`.
See [the interaction contract](PLUGIN_INTERACTION.md).
`mk/plugin.mk` supplies reusable build conventions and `KWMOD_SDK_ROOT`; it does
not include or compile another component's implementation.
