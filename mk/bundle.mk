# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: MIT
# Include after defining BUNDLE_FILES (source=engine-root-relative-path).
include $(KWMOD_SDK_ROOT)/mk/stage_tool.mk
BUNDLE_DIR ?= $(OUT_DIR)/bundle
# Keep each overlay's documentation/licensing at non-colliding paths.
BUNDLE_FILES += $(REPO_ROOT)/README.md=docs/$(notdir $(REPO_ROOT))/README.md
# Every binary overlay carries its own license and permissions; namespaced
# destinations prevent composition from replacing another component's terms.
BUNDLE_LEGAL_FILES := $(wildcard $(REPO_ROOT)/LICENSE $(REPO_ROOT)/LICENSING.md $(REPO_ROOT)/SDK_LICENSE $(REPO_ROOT)/REUSE.toml $(REPO_ROOT)/CONTRIBUTING.md)
BUNDLE_FILES += $(foreach file,$(BUNDLE_LEGAL_FILES),$(file)=docs/$(notdir $(REPO_ROOT))/$(notdir $(file)))
BUNDLE_FILES += $(foreach file,$(BUNDLE_LEGAL_FILES),$(file)=licenses/$(notdir $(REPO_ROOT))/$(notdir $(file)))
BUNDLE_FILES += $(foreach file,$(wildcard $(REPO_ROOT)/LICENSES/*),$(file)=docs/$(notdir $(REPO_ROOT))/LICENSES/$(notdir $(file)))
BUNDLE_FILES += $(foreach file,$(wildcard $(REPO_ROOT)/LICENSES/*),$(file)=licenses/$(notdir $(REPO_ROOT))/LICENSES/$(notdir $(file)))
# Preserve relative links from the repository README and user documentation.
BUNDLE_FILES += $(foreach file,$(wildcard $(REPO_ROOT)/docs/*),$(file)=docs/$(notdir $(REPO_ROOT))/docs/$(notdir $(file)))
BUNDLE_FILES += $(foreach file,$(wildcard $(REPO_ROOT)/THIRD_PARTY_NOTICES.md),$(file)=docs/$(notdir $(REPO_ROOT))/$(notdir $(file)))
BUNDLE_FILES += $(foreach file,$(wildcard $(REPO_ROOT)/notices/*),$(file)=licenses/$(notdir $(REPO_ROOT))/$(notdir $(file)))
BUNDLE_FILES += $(foreach file,$(wildcard $(REPO_ROOT)/notices/*),$(file)=docs/$(notdir $(REPO_ROOT))/notices/$(notdir $(file)))
.PHONY: bundle stage
bundle: all kwmod-stage-tool
	"$(KWMOD_STAGE_TOOL)" --output "$(BUNDLE_DIR)" --component "$(notdir $(REPO_ROOT))" $(foreach file,$(BUNDLE_FILES),--file "$(file)")
stage: bundle
