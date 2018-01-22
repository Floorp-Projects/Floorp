#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Temporary placeholders to call new buildbot script locations until
# buildbot master config can be pointed to new location.

exec $(dirname $0)/buildbot/bb_main_builder.sh "$@"
