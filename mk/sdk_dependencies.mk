# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: MIT
# Include after BIN_TARGETS and ENGINE_ARTIFACTS are defined. An order-only
# recursive SDK build alone can leave GNU Make's cached artifact mtime stale.
# A normal digest-stamp dependency forces relinking in the SAME invocation,
# while unchanged SDK outputs leave the stamp and consumer binaries untouched.
SDK_ARTIFACT_STAMP := $(OUT_DIR)/sdk-artifacts.sha256
.PHONY: sdk-artifact-force
sdk-artifact-force:
$(SDK_ARTIFACT_STAMP): sdk-artifact-force
	@test "$(abspath $(KWMOD_BIN_DIR)/..)" = "$(abspath $(KWMOD_LIB_DIR)/..)" || { echo 'SDK bin/lib must share one build root.' >&2; exit 2; }
	$(MAKE) -C "$(KWMOD_SDK_ROOT)" BUILD_DIR="$(abspath $(KWMOD_BIN_DIR)/..)" all
	@mkdir -p "$(@D)"
	@stamp_tmp=$$(mktemp "$@.XXXXXX"); \
	trap '$(RM) "$$stamp_tmp"' EXIT HUP INT TERM; \
	( $(foreach artifact,$(ENGINE_ARTIFACTS),sha256sum "$(artifact)" && ) true ) > "$$stamp_tmp" && \
	{ cmp -s "$$stamp_tmp" "$@" || mv -f "$$stamp_tmp" "$@"; }
$(BIN_TARGETS): $(SDK_ARTIFACT_STAMP)
