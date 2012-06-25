#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Buildbot annotator script for the FYI waterfall.  Compile,
# experimental compile, run tests, ...

# SHERIFF: there should be no need to disable this bot.
# The FYI waterfall does not close the tree.

ROOT=$(cd "$(dirname $0)"; pwd)
. "${ROOT}"/buildbot_functions.sh

bb_baseline_setup "${ROOT}"/../..
bb_compile
bb_compile_experimental
bb_run_tests


