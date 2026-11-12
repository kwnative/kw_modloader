#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: LicenseRef-KW-Modloader
set -euo pipefail
sdk_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
test_root=$(mktemp -d "${TMPDIR:-/tmp}/kw-integration-test.XXXXXXXX")
trap 'rm -rf -- "$test_root"' EXIT
unset MAKEFLAGS MFLAGS MAKEOVERRIDES MAKECMDGOALS KWMOD_BUNDLE_PROVIDERS WITH_TEST_INTEGRATION BUILD_DIR KWMOD_BIN_DIR KWMOD_LIB_DIR
mkdir -p "$test_root/provider/include/example"
cat > "$test_root/Makefile" <<'MAKEFILE'
REPO_ROOT := $(CURDIR)
include $(KWMOD_SDK_ROOT)/mk/plugin.mk
.DEFAULT_GOAL := all
PROVIDER_HEADER ?= $(REPO_ROOT)/provider/include/example/policy.h
WITH_TEST_INTEGRATION ?= $(call kwmod_bundle_integration_default,$(PROVIDER_HEADER),test-provider)
ifneq ($(filter $(WITH_TEST_INTEGRATION),0 1),$(WITH_TEST_INTEGRATION))
$(error invalid test integration flag)
endif
ifeq ($(WITH_TEST_INTEGRATION),1)
ifeq ($(wildcard $(PROVIDER_HEADER)),)
$(error missing explicit provider header)
endif
endif
INTEGRATION_SETTINGS := TEST=$(WITH_TEST_INTEGRATION):$(PROVIDER_HEADER)
ALL_OBJECTS := $(OBJ_DIR)/consumer.o
.PHONY: all bundle stage
all: $(ALL_OBJECTS)
bundle: all
stage: bundle
$(ALL_OBJECTS):
	@mkdir -p "$(@D)"
	@printf '%s\n' '$(WITH_TEST_INTEGRATION)' > "$@"
include $(KWMOD_SDK_ROOT)/mk/integration.mk
MAKEFILE
fixture_reset()
{
    rm -rf -- "$test_root/build"
    printf '/* Optional provider fixture. */\n' > "$test_root/provider/include/example/policy.h"
}
run_make()
{
    make --no-print-directory -C "$test_root" KWMOD_SDK_ROOT="$sdk_root" "$@" > "$test_root/make.log" 2>&1
}
expect_flag()
{
    if [[ $(cat "$test_root/build/obj/consumer.o") != "$1" ]]; then
        cat "$test_root/make.log"
        echo "FAIL integration flag, expected $1" >&2
        exit 1
    fi
}
fixture_reset; run_make; expect_flag 0
fixture_reset; run_make bundle; expect_flag 1
fixture_reset; run_make stage; expect_flag 1
fixture_reset; run_make bundle 'KWMOD_BUNDLE_PROVIDERS=consumer test-provider'; expect_flag 1
fixture_reset; run_make bundle 'KWMOD_BUNDLE_PROVIDERS=consumer another-provider'; expect_flag 0
fixture_reset; run_make bundle KWMOD_BUNDLE_PROVIDERS=; expect_flag 0
fixture_reset; rm "$test_root/provider/include/example/policy.h"; run_make bundle; expect_flag 0
fixture_reset; run_make bundle WITH_TEST_INTEGRATION=0; expect_flag 0
fixture_reset; WITH_TEST_INTEGRATION=0 run_make bundle; expect_flag 0
fixture_reset; run_make all WITH_TEST_INTEGRATION=1; expect_flag 1
fixture_reset; rm "$test_root/provider/include/example/policy.h"
if run_make bundle WITH_TEST_INTEGRATION=1; then
    echo 'FAIL missing explicit provider was accepted' >&2
    exit 1
fi
[[ $(cat "$test_root/make.log") == *'missing explicit provider header'* ]]
fixture_reset; run_make all; expect_flag 0
run_make bundle; expect_flag 1
cp -p "$test_root/build/obj/consumer.o" "$test_root/object-before"
cp -p "$test_root/build/obj/integration-settings" "$test_root/stamp-before"
run_make bundle
expect_flag 1
for pair in 'consumer.o object-before' 'integration-settings stamp-before'; do
    read -r current before <<< "$pair"
    if [[ "$test_root/build/obj/$current" -nt "$test_root/$before" || "$test_root/$before" -nt "$test_root/build/obj/$current" ]]; then
        echo "FAIL unchanged integration rebuilt $current" >&2
        exit 1
    fi
done
run_make all; expect_flag 0
fixture_reset; run_make bundle KWMOD_BUNDLE_PROVIDERS=test-provider; expect_flag 1
run_make bundle KWMOD_BUNDLE_PROVIDERS=another-provider; expect_flag 0
echo 'PASS optional integration cases=13; transitions, overrides and unchanged timestamps'
