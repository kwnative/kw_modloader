# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: MIT
# Native build tool, separate from the x86 game DLLs. No runtime plugin dependency.
ifndef KWMOD_STAGE_TOOL_RULES
KWMOD_STAGE_TOOL_RULES := 1
KWMOD_STAGE_BUILD_DIR ?= $(KWMOD_SDK_ROOT)/build
KWMOD_HOST_EXE_SUFFIX := $(if $(filter Windows_NT,$(OS)),.exe,)
KWMOD_STAGE_TOOL ?= $(KWMOD_STAGE_BUILD_DIR)/host/kw_stage_bundle$(KWMOD_HOST_EXE_SUFFIX)
.PHONY: kwmod-stage-tool
kwmod-stage-tool:
	$(MAKE) -C "$(KWMOD_SDK_ROOT)" BUILD_DIR="$(KWMOD_STAGE_BUILD_DIR)" stage-tool
endif
