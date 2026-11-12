# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: LicenseRef-KW-Modloader
REPO_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KWMOD_SDK_ROOT ?= $(REPO_ROOT)
include $(REPO_ROOT)/mk/plugin.mk
include $(REPO_ROOT)/mk/stage_tool.mk
HOST_CXX ?= $(if $(filter Windows_NT,$(OS)),$(WINDOWS_CXX),c++)
HOST_CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
HOST_TOOL_LDFLAGS ?= $(if $(filter Windows_NT,$(OS)),-static,) -static-libgcc -static-libstdc++
STAGE_TOOL_TARGET := $(OUT_DIR)/host/kw_stage_bundle$(KWMOD_HOST_EXE_SUFFIX)
STAGE_TEST_TARGET := $(OUT_DIR)/tests/test_stage_bundle$(KWMOD_HOST_EXE_SUFFIX)
STAGE_TOOL_SOURCES := $(REPO_ROOT)/tools/stage_bundle.cpp $(REPO_ROOT)/tools/bundle_stage.hpp $(REPO_ROOT)/tools/bundle_format.hpp
.DEFAULT_GOAL := all
.PHONY: test-selection-batches
test-selection-batches:
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -I"$(REPO_ROOT)/include" \
	    "$(REPO_ROOT)/tests/test_selection_batches.c" -o "$(OUT_DIR)/tests/test_selection_batches"
	"$(OUT_DIR)/tests/test_selection_batches"
	$(WINDOWS_CC) -std=c11 -O2 -Wall -Wextra -Werror -static -static-libgcc -I"$(REPO_ROOT)/include" \
	    "$(REPO_ROOT)/tests/test_selection_batches.c" -o "$(OUT_DIR)/tests/test_selection_batches.exe"
LOADER_OBJECTS := $(call objects,src/loader/dinput8.c)
RUNTIME_OBJECTS := $(call objects,src/runtime/runtime.c src/core/core.c)
EXAMPLE_OBJECTS := $(call objects,plugin_example/src/minimal.c)
ENGINE_OBJECTS := $(call objects,src/engine/cache.c)
CONFIG_OBJECTS := $(call objects,src/config/config.c)
TIMING_OBJECTS := $(call objects,src/timing/common.c src/timing/game_layout.c)
$(TIMING_OBJECTS): CFLAGS := $(filter-out -Wpedantic,$(CFLAGS))
ALL_OBJECTS := $(sort $(LOADER_OBJECTS) $(RUNTIME_OBJECTS) $(EXAMPLE_OBJECTS) $(ENGINE_OBJECTS) $(CONFIG_OBJECTS) $(TIMING_OBJECTS))
TARGETS := $(BIN_DIR)/dinput8.dll $(BIN_DIR)/kw_runtime.dll $(BIN_DIR)/kw_engine.dll $(LIB_DIR)/libkw_engine.a $(LIB_DIR)/libkw_config.a $(LIB_DIR)/libkw_timing.a
.PHONY: all example clean test-bundle test-bundle-integrations test-no-python test-config \
    test-hook-context test-map-sources test-prefix-index test-patch-registry
all: $(TARGETS)
example: $(BIN_DIR)/kw_example.dll
test-patch-registry:
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -I"$(REPO_ROOT)/include" \
	    "$(REPO_ROOT)/tests/test_patch_registry.c" -o "$(OUT_DIR)/tests/test_patch_registry"
	"$(OUT_DIR)/tests/test_patch_registry"
test-bundle: test-no-python $(STAGE_TEST_TARGET)
	"$(STAGE_TEST_TARGET)"
test-no-python:
	@unexpected=$$(find "$(REPO_ROOT)/src" "$(REPO_ROOT)/include" "$(REPO_ROOT)/tools" "$(REPO_ROOT)/tests" \
	    -type f \( -iname '*.py' -o -iname '*.pyw' -o -iname '*.pyi' -o -iname '*.pyc' -o -iname '*.pyo' -o -iname '*.ipynb' \) -print) || exit 1; \
	if test -n "$$unexpected"; then printf 'Unexpected source files:\n%s\n' "$$unexpected" >&2; exit 1; fi; \
	printf 'PASS no Python source files\n'
test-bundle-integrations:
	bash "$(REPO_ROOT)/tests/test_bundle_integrations.sh"
.PHONY: stage-tool
stage-tool: $(STAGE_TOOL_TARGET)
$(STAGE_TOOL_TARGET): $(STAGE_TOOL_SOURCES)
	@mkdir -p "$(@D)"
	$(HOST_CXX) $(HOST_CXXFLAGS) $(HOST_TOOL_LDFLAGS) $(if $(filter Windows_NT,$(OS)),-municode,) "$(REPO_ROOT)/tools/stage_bundle.cpp" -o "$@"
$(STAGE_TEST_TARGET): $(STAGE_TOOL_SOURCES) $(REPO_ROOT)/tests/test_stage_bundle.cpp
	@mkdir -p "$(@D)"
	$(HOST_CXX) $(HOST_CXXFLAGS) $(HOST_TOOL_LDFLAGS) -DKWMOD_STAGE_NO_MAIN "$(REPO_ROOT)/tools/stage_bundle.cpp" \
	    "$(REPO_ROOT)/tests/test_stage_bundle.cpp" -o "$@"
test-config:
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -std=c11 -Wall -Wextra -Werror -I"$(REPO_ROOT)/include" "$(REPO_ROOT)/tests/test_config.c" "$(REPO_ROOT)/src/config/config.c" -o "$(OUT_DIR)/tests/test_config"
	"$(OUT_DIR)/tests/test_config"
test-prefix-index:
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$(REPO_ROOT)/include" \
	    "$(REPO_ROOT)/tests/test_engine_prefix_index.c" -o "$(OUT_DIR)/tests/test_engine_prefix_index"
	"$(OUT_DIR)/tests/test_engine_prefix_index"
test-hook-context:
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -m32 -std=c11 -O2 -Wall -Wextra -Werror -I"$(REPO_ROOT)/tests/platform" -I"$(REPO_ROOT)/include" "$(REPO_ROOT)/tests/test_hook_context.c" -o "$(OUT_DIR)/tests/test_hook_context"
	qemu-i386 "$(OUT_DIR)/tests/test_hook_context"
# Explicit private image argument; never a release-build prerequisite.
ifeq ($(OS),Windows_NT)
MAP_SOURCE_TEST_LINK_FLAGS ?= -Wl,--image-base,0x20000000
endif
test-map-sources:
	@test -n "$(ENGINE_IMAGE)" || { echo 'Set ENGINE_IMAGE to a compatible private game executable.' >&2; exit 2; }
	@mkdir -p "$(OUT_DIR)/tests"
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$(REPO_ROOT)/include" \
	    $(MAP_SOURCE_TEST_LINK_FLAGS) \
	    "$(REPO_ROOT)/tests/test_map_source_bindings.c" -o "$(OUT_DIR)/tests/test_map_source_bindings"
	"$(OUT_DIR)/tests/test_map_source_bindings" "$(ENGINE_IMAGE)"
$(BIN_DIR)/dinput8.dll: $(LOADER_OBJECTS)
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(DLL_FLAGS) $^ -o "$@"
$(BIN_DIR)/kw_runtime.dll: $(RUNTIME_OBJECTS)
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(DLL_FLAGS) $^ -o "$@"
$(BIN_DIR)/kw_example.dll: $(EXAMPLE_OBJECTS)
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(DLL_FLAGS) $^ -o "$@"
$(BIN_DIR)/kw_engine.dll $(LIB_DIR)/libkw_engine.a &: $(ENGINE_OBJECTS) $(REPO_ROOT)/src/engine/engine.def
	@mkdir -p "$(BIN_DIR)" "$(LIB_DIR)"
	$(WINDOWS_CC) $(DLL_FLAGS) -Wl,--out-implib,$(LIB_DIR)/libkw_engine.a $^ -o "$(BIN_DIR)/kw_engine.dll"
$(LIB_DIR)/libkw_config.a: $(CONFIG_OBJECTS)
	@mkdir -p "$(@D)"
	$(WINDOWS_AR) rcs "$@" $^
$(LIB_DIR)/libkw_timing.a: $(TIMING_OBJECTS)
	@mkdir -p "$(@D)"
	$(WINDOWS_AR) rcs "$@" $^
clean:
	$(RM) $(TARGETS) $(BIN_DIR)/kw_example.dll $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d) $(STAGE_TOOL_TARGET) $(STAGE_TEST_TARGET)
-include $(ALL_OBJECTS:.o=.d)

BUNDLE_FILES := $(BIN_DIR)/dinput8.dll=dinput8.dll \
    $(BIN_DIR)/kw_runtime.dll=kw_runtime.dll \
    $(BIN_DIR)/kw_engine.dll=kw_engine.dll \
    $(REPO_ROOT)/docs/BUNDLES.md=README-bundle.md
include $(KWMOD_SDK_ROOT)/mk/bundle.mk
