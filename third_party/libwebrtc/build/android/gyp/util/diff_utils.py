#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import difflib
from util import build_utils


def _SkipOmitted(line):
  """
  Skip lines that are to be intentionally omitted from the expectations file.

  This is required when the file to be compared against expectations contains
  a line that changes from build to build because - for instance - it contains
  version information.
  """
  if line.rstrip().endswith('# OMIT FROM EXPECTATIONS'):
    return '# THIS LINE WAS OMITTED\n'
  return line


def _GenerateDiffWithOnlyAdditons(expected_path, actual_data):
  """Generate a diff that only contains additions"""
  # Ignore blank lines when creating the diff to cut down on whitespace-only
  # lines in the diff. Also remove trailing whitespaces and add the new lines
  # manually (ndiff expects new lines but we don't care about trailing
  # whitespace).
  with open(expected_path) as expected:
    expected_lines = [l for l in expected.readlines() if l.strip()]
  actual_lines = [
      '{}\n'.format(l.rstrip()) for l in actual_data.splitlines() if l.strip()
  ]

  diff = difflib.ndiff(expected_lines, actual_lines)
  filtered_diff = (l for l in diff if l.startswith('+'))
  return ''.join(filtered_diff)


def _DiffFileContents(expected_path, actual_data):
  """Check file contents for equality and return the diff or None."""
  # Remove all trailing whitespace and add it explicitly in the end.
  with open(expected_path) as f_expected:
    expected_lines = [l.rstrip() for l in f_expected.readlines()]
  actual_lines = [
      _SkipOmitted(line).rstrip() for line in actual_data.splitlines()
  ]

  if expected_lines == actual_lines:
    return None

  expected_path = os.path.relpath(expected_path, build_utils.DIR_SOURCE_ROOT)

  diff = difflib.unified_diff(
      expected_lines,
      actual_lines,
      fromfile=os.path.join('before', expected_path),
      tofile=os.path.join('after', expected_path),
      n=0,
      lineterm='',
  )

  return '\n'.join(diff)


def AddCommandLineFlags(parser):
  group = parser.add_argument_group('Expectations')
  group.add_argument(
      '--expected-file',
      help='Expected contents for the check. If --expected-file-base  is set, '
      'this is a diff of --actual-file and --expected-file-base.')
  group.add_argument(
      '--expected-file-base',
      help='File to diff against before comparing to --expected-file.')
  group.add_argument('--actual-file',
                     help='Path to write actual file (for reference).')
  group.add_argument('--failure-file',
                     help='Write to this file if expectations fail.')
  group.add_argument('--fail-on-expectations',
                     action="store_true",
                     help='Fail on expectation mismatches.')
  group.add_argument('--only-verify-expectations',
                     action='store_true',
                     help='Verify the expectation and exit.')


def CheckExpectations(actual_data, options):
  with build_utils.AtomicOutput(options.actual_file) as f:
    f.write(actual_data)
  if options.expected_file_base:
    actual_data = _GenerateDiffWithOnlyAdditons(options.expected_file_base,
                                                actual_data)
  diff_text = _DiffFileContents(options.expected_file, actual_data)

  if not diff_text:
    fail_msg = ''
  else:
    fail_msg = """
Expectations need updating:
https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/android/expectations/README.md

LogDog tip: Use "Raw log" or "Switch to lite mode" before copying:
https://bugs.chromium.org/p/chromium/issues/detail?id=984616

To update expectations, run:
########### START ###########
 patch -p1 <<'END_DIFF'
{}
END_DIFF
############ END ############
""".format(diff_text)

    sys.stderr.write(fail_msg)

  if options.failure_file:
    with open(options.failure_file, 'w') as f:
      f.write(fail_msg)
  if fail_msg and options.fail_on_expectations:
    sys.exit(1)
