# Minimal independent plugin

This example is MIT-licensed, including its source, manifest and build template.
See [LICENSE](LICENSE). You may use it as the starting point for a proprietary
plugin. The loader/runtime retain their separate GPL terms and plugin permission;
copying this example does not permit copying GPL-covered runtime implementation.

`src/minimal.c` uses only `<kwf/plugin.h>` and exports `KwPluginQuery`.
From this directory run `make`. If copying this example outside the loader
checkout, pass `KWMOD_SDK_ROOT=/path/to/kw_modloader` as an environment variable
or Make argument. No separate SDK, import library, AI or game image is needed.
The result is `build/bin/kw_example.dll`; place it with `example.kwplugin` in a
runtime's `plugins/` directory only when explicitly testing the example.
The manifest is disabled by default. Change both descriptor and manifest IDs
when using it as the starting point for a different plugin.

`make bundle` (alias `make stage`) creates `build/bundle/plugins/` with the DLL
and its disabled manifest, plus documentation and a packaging inventory.
