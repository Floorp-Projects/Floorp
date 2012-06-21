#!/bin/sh

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

THISDIR=$(dirname "${0}")
PYTHONPATH="${THISDIR}/../valgrind:${THISDIR}/../python/google" exec "${THISDIR}/chrome_tests.py" "${@}"
