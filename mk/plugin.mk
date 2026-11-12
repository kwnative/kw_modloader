# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: MIT
# Shared build conventions for the combined loader/engine SDK.
# Set REPO_ROOT in the consuming Makefile before including this file.
ifndef REPO_ROOT
$(error REPO_ROOT must identify the consuming repository)
endif
WINDOWS_CC ?= i686-w64-mingw32-gcc
WINDOWS_CXX ?= i686-w64-mingw32-g++
WINDOWS_AR ?= i686-w64-mingw32-ar
BUILD_DIR ?= $(REPO_ROOT)/build
OUT_DIR := $(abspath $(BUILD_DIR))
BIN_DIR := $(OUT_DIR)/bin
LIB_DIR := $(OUT_DIR)/lib
OBJ_DIR := $(OUT_DIR)/obj
CONFIG_DIR := $(OUT_DIR)/config
KWMOD_SDK_ROOT ?= $(abspath $(REPO_ROOT)/../kw_modloader)
KWMOD_BIN_DIR ?= $(KWMOD_SDK_ROOT)/build/bin
KWMOD_LIB_DIR ?= $(KWMOD_SDK_ROOT)/build/lib
KWMOD_ENGINE_DLL := $(KWMOD_BIN_DIR)/kw_engine.dll
KWMOD_ENGINE_IMPORT_LIB := $(KWMOD_LIB_DIR)/libkw_engine.a
KWMOD_CONFIG_LIB := $(KWMOD_LIB_DIR)/libkw_config.a
KWMOD_TIMING_LIB := $(KWMOD_LIB_DIR)/libkw_timing.a
# Bundle overlays cooperate with selected providers whose headers are available.
# Ordinary standalone builds still need only this SDK. This generic helper
# knows no plugin protocols and never stages, imports or enables a provider.
# Consumers retain explicit WITH_* overrides (including environment values).
# A composition passes selected plugin repository names; an overlay may use
# any available provider headers without bundling/installing that provider.
kwmod_bundle_provider_selected = $(if $(filter undefined,$(origin KWMOD_BUNDLE_PROVIDERS)),1,$(filter $(1),$(KWMOD_BUNDLE_PROVIDERS)))
kwmod_bundle_integration_default = $(if $(and $(filter bundle stage,$(MAKECMDGOALS)),$(wildcard $(1)),\
    $(call kwmod_bundle_provider_selected,$(2))),1,0)
CPPFLAGS += -I"$(REPO_ROOT)/include" -I"$(KWMOD_SDK_ROOT)/include"
# Do not embed the builder's checkout paths in __FILE__ or debug information.
CPPFLAGS += -ffile-prefix-map="$(REPO_ROOT)=." -ffile-prefix-map="$(KWMOD_SDK_ROOT)=kw_modloader"
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
# Keep compiler runtime libraries static; Windows system DLLs remain required.
DLL_FLAGS := -shared -static -static-libgcc -Wl,--kill-at
EXE_FLAGS := -static -static-libgcc -static-libstdc++
objects = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(1)))
SDK_PATH_STAMP := $(OBJ_DIR)/sdk-path
.PHONY: sdk-path-force
sdk-path-force:
$(SDK_PATH_STAMP): sdk-path-force
	@mkdir -p "$(@D)"
	@stamp_tmp=$$(mktemp "$@.XXXXXX"); \
	trap '$(RM) "$$stamp_tmp"' EXIT HUP INT TERM; \
	printf '%s\n' '$(KWMOD_SDK_ROOT)' '$(REPO_ROOT)' > "$$stamp_tmp" && \
	{ cmp -s "$$stamp_tmp" "$@" || mv -f "$$stamp_tmp" "$@"; }

.PHONY: sdk-check sdk-paths sdk-build
sdk-check:
	@test -f "$(KWMOD_SDK_ROOT)/include/kwf/plugin.h" || { echo 'Set KWMOD_SDK_ROOT to the loader SDK repository.' >&2; exit 2; }
sdk-paths:
	@echo 'Loader headers: $(KWMOD_SDK_ROOT)/include'
	@echo 'Loader binaries: $(KWMOD_BIN_DIR)'
	@echo 'SDK libraries: $(KWMOD_LIB_DIR)'
sdk-build: sdk-check
	@test "$(abspath $(KWMOD_BIN_DIR)/..)" = "$(abspath $(KWMOD_LIB_DIR)/..)" || { echo 'SDK bin/lib must share one build root.' >&2; exit 2; }
	$(MAKE) -C "$(KWMOD_SDK_ROOT)" BUILD_DIR="$(abspath $(KWMOD_BIN_DIR)/..)" all

$(OBJ_DIR)/%.c.o: $(REPO_ROOT)/%.c $(SDK_PATH_STAMP) | sdk-check
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c "$<" -o "$@"
$(OBJ_DIR)/%.cpp.o: $(REPO_ROOT)/%.cpp $(SDK_PATH_STAMP) | sdk-check
	@mkdir -p "$(@D)"
	$(WINDOWS_CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"
$(OBJ_DIR)/%.S.o: $(REPO_ROOT)/%.S $(SDK_PATH_STAMP) | sdk-check
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(CPPFLAGS) -MMD -MP -c "$<" -o "$@"
