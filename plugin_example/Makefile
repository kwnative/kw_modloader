# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: MIT
REPO_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KWMOD_SDK_ROOT ?= $(abspath $(REPO_ROOT)/..)
include $(KWMOD_SDK_ROOT)/mk/plugin.mk
.DEFAULT_GOAL := all
ALL_OBJECTS := $(call objects,src/minimal.c)
.PHONY: all clean
all: $(BIN_DIR)/kw_example.dll
$(BIN_DIR)/kw_example.dll: $(ALL_OBJECTS)
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(DLL_FLAGS) $^ -o "$@"
clean:
	$(RM) $(BIN_DIR)/kw_example.dll $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d)
-include $(ALL_OBJECTS:.o=.d)

BUNDLE_FILES := $(BIN_DIR)/kw_example.dll=plugins/kw_example.dll \
    $(REPO_ROOT)/example.kwplugin=plugins/example.kwplugin \
    $(KWMOD_SDK_ROOT)/notices/PLUGIN_EXCEPTION.txt=licenses/plugin_example/PLUGIN_EXCEPTION.txt
include $(KWMOD_SDK_ROOT)/mk/bundle.mk
