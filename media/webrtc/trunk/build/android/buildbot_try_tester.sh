#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Buildbot annotator script for tester half of android trybots
BB_SRC_ROOT="$(cd "$(dirname $0)/../.."; pwd)"
. "${BB_SRC_ROOT}/build/android/buildbot_functions.sh"

# SHERIFF: if you need to quickly turn "android" trybots green,
# uncomment the next line (and send appropriate email out):
## bb_force_bot_green_and_exit
# You will also need to change buildbot_try_builder.sh

bb_baseline_setup "$BB_SRC_ROOT" "$@"
bb_extract_build
bb_reboot_phones
bb_run_tests
bb_run_content_shell_instrumentation_test
