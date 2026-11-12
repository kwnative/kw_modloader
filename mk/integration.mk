# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: MIT
# Optional provider headers are never implicit mandatory dependencies. Consumers
# define flags and add only the selected provider's public include root.
.PHONY: integration-force
integration-force:
INTEGRATION_STAMP := $(OBJ_DIR)/integration-settings
$(INTEGRATION_STAMP): integration-force
	@mkdir -p "$(@D)"
	@stamp_tmp=$$(mktemp "$@.XXXXXX"); \
	trap '$(RM) "$$stamp_tmp"' EXIT HUP INT TERM; \
	printf '%s\n' '$(INTEGRATION_SETTINGS)' > "$$stamp_tmp" && \
	{ cmp -s "$$stamp_tmp" "$@" || mv -f "$$stamp_tmp" "$@"; }
$(ALL_OBJECTS): $(INTEGRATION_STAMP)
