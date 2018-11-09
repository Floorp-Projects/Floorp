#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs both the Python and Java tests."""

import optparse
import sys
import time

from pylib import apk_info
from pylib import buildbot_report
from pylib import ports
from pylib import run_java_tests
from pylib import run_python_tests
from pylib import run_tests_helper
from pylib import test_options_parser
from pylib.test_result import TestResults


def SummarizeResults(java_results, python_results, annotation, build_type):
  """Summarize the results from the various test types.

  Args:
    java_results: a TestResults object with java test case results.
    python_results: a TestResults object with python test case results.
    annotation: the annotation used for these results.
    build_type: 'Release' or 'Debug'.

  Returns:
    A tuple (all_results, summary_string, num_failing)
  """
  all_results = TestResults.FromTestResults([java_results, python_results])
  summary_string = all_results.LogFull('Instrumentation', annotation,
                                       build_type)
  num_failing = (len(all_results.failed) + len(all_results.crashed) +
                 len(all_results.unknown))
  return all_results, summary_string, num_failing


def DispatchInstrumentationTests(options):
  """Dispatches the Java and Python instrumentation tests, sharding if possible.

  Uses the logging module to print the combined final results and
  summary of the Java and Python tests. If the java_only option is set, only
  the Java tests run. If the python_only option is set, only the python tests
  run. If neither are set, run both Java and Python tests.

  Args:
    options: command-line options for running the Java and Python tests.

  Returns:
    An integer representing the number of failing tests.
  """
  # Reset the test port allocation. It's important to do it before starting
  # to dispatch any tests.
  if not ports.ResetTestServerPortAllocation():
    raise Exception('Failed to reset test server port.')
  start_date = int(time.time() * 1000)
  java_results = TestResults()
  python_results = TestResults()

  if options.run_java_tests:
    java_results = run_java_tests.DispatchJavaTests(
        options,
        [apk_info.ApkInfo(options.test_apk_path, options.test_apk_jar_path)])
  if options.run_python_tests:
    python_results = run_python_tests.DispatchPythonTests(options)

  all_results, summary_string, num_failing = SummarizeResults(
      java_results, python_results, options.annotation, options.build_type)
  return num_failing


def main(argv):
  option_parser = optparse.OptionParser()
  test_options_parser.AddInstrumentationOptions(option_parser)
  options, args = option_parser.parse_args(argv)
  test_options_parser.ValidateInstrumentationOptions(option_parser, options,
                                                     args)

  run_tests_helper.SetLogLevel(options.verbose_count)
  buildbot_report.PrintNamedStep(
      'Instrumentation tests: %s - %s' % (', '.join(options.annotation),
                                          options.test_apk))
  return DispatchInstrumentationTests(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
