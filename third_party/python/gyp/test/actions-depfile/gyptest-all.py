#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that depfile fields are output in ninja rules."""

import TestGyp
import os

test = TestGyp.TestGyp()

if test.format == 'ninja':
  test.run_gyp('depfile.gyp')
  contents = open(test.built_file_path('obj/depfile_target.ninja')).read()

  expected = [
    'depfile = depfile_action.d',
    'depfile = ' + os.path.join(
      'obj', 'depfile_target.gen/depfile_action_intermediate_dir.d'),
  ]
  test.must_contain_all_lines(contents, expected)

  test.build('depfile.gyp')
  test.built_file_must_exist('depfile_action.d')
  test.built_file_must_exist(
    'obj/depfile_target.gen/depfile_action_intermediate_dir.d')

  test.pass_test()
