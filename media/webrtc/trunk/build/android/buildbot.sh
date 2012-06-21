#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Currently used as the entry point by both trybot and FYI bot.
# TODO(jrg): redirect those bots to buildbot_try_compile.sh and
# buildbot_fyi.sh, then delete this file.


ROOT=$(cd "$(dirname $0)"; pwd)
if [ "${TRYBOT:-0}" = 1 ] ; then
  exec $ROOT/buildbot_try_compile.sh
else
  exec $ROOT/buildbot_fyi.sh
fi
