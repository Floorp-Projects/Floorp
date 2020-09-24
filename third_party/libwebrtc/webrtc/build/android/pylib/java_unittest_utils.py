# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file is imported by python tests ran by run_python_tests.py."""

import os

import android_commands
from run_java_tests import TestRunner


def _GetPackageName(fname):
  """Extracts the package name from the test file path."""
  base_root = os.path.join('com', 'google', 'android')
  dirname = os.path.dirname(fname)
  package = dirname[dirname.rfind(base_root):]
  return package.replace(os.sep, '.')


def RunJavaTest(fname, suite, test, ports_to_forward):
  device = android_commands.GetAttachedDevices()[0]
  package_name = _GetPackageName(fname)
  test = package_name + '.' + suite + '#' + test
  java_test_runner = TestRunner(False, device, [test], False, False, False,
                                False, 0, ports_to_forward)
  return java_test_runner.Run()
