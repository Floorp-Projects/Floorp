#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies nested dependent_settings directives project generation.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp("all-dependent-settings.gyp")
test.run_gyp("direct-dependent-settings.gyp")

test.pass_test()
