# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses options for the instrumentation tests."""

import constants
import optparse
import os
import sys

_SDK_OUT_DIR = os.path.join(constants.CHROME_DIR, 'out')


def AddBuildTypeOption(option_parser):
  """Decorates OptionParser with build type option."""
  default_build_type = 'Debug'
  if 'BUILDTYPE' in os.environ:
    default_build_type = os.environ['BUILDTYPE']
  option_parser.add_option('--debug', action='store_const', const='Debug',
                           dest='build_type', default=default_build_type,
                           help='If set, run test suites under out/Debug. '
                                'Default is env var BUILDTYPE or Debug')
  option_parser.add_option('--release', action='store_const', const='Release',
                           dest='build_type',
                           help='If set, run test suites under out/Release. '
                                'Default is env var BUILDTYPE or Debug.')

def AddInstallAPKOption(option_parser):
  """Decorates OptionParser with apk option used to install the APK."""
  option_parser.add_option('--apk',
                           help=('The name of the apk containing the '
                                 ' application (with the .apk extension).'))
  option_parser.add_option('--apk_package',
                           help=('The package name used by the apk containing '
                                 'the application.'))

def AddTestRunnerOptions(option_parser, default_timeout=60):
  """Decorates OptionParser with options applicable to all tests."""

  option_parser.add_option('-t', dest='timeout',
                           help='Timeout to wait for each test',
                           type='int',
                           default=default_timeout)
  option_parser.add_option('-c', dest='cleanup_test_files',
                           help='Cleanup test files on the device after run',
                           action='store_true')
  option_parser.add_option('-v',
                           '--verbose',
                           dest='verbose_count',
                           default=0,
                           action='count',
                           help='Verbose level (multiple times for more)')
  profilers = ['devicestatsmonitor', 'chrometrace', 'dumpheap', 'smaps',
               'traceview']
  option_parser.add_option('--profiler', dest='profilers', action='append',
                           choices=profilers,
                           help='Profiling tool to run during test. '
                           'Pass multiple times to run multiple profilers. '
                           'Available profilers: %s' % profilers)
  option_parser.add_option('--tool',
                           dest='tool',
                           help='Run the test under a tool '
                           '(use --tool help to list them)')
  AddBuildTypeOption(option_parser)


def AddInstrumentationOptions(option_parser):
  """Decorates OptionParser with instrumentation tests options."""

  AddTestRunnerOptions(option_parser)
  option_parser.add_option('-w', '--wait_debugger', dest='wait_for_debugger',
                           action='store_true', help='Wait for debugger.')
  option_parser.add_option('-I', dest='install_apk', help='Install APK.',
                           action='store_true')
  option_parser.add_option('-f', '--test_filter',
                           help='Test filter (if not fully qualified, '
                           'will run all matches).')
  option_parser.add_option('-A', '--annotation', dest='annotation_str',
                           help=('Run only tests with any of the given '
                                 'annotations. '
                                 'An annotation can be either a key or a '
                                 'key-values pair. '
                                 'A test that has no annotation is '
                                 'considered "SmallTest".'))
  option_parser.add_option('-j', '--java_only', action='store_true',
                           help='Run only the Java tests.')
  option_parser.add_option('-p', '--python_only', action='store_true',
                           help='Run only the Python tests.')
  option_parser.add_option('-n', '--run_count', type='int',
                           dest='number_of_runs', default=1,
                           help=('How many times to run each test, regardless '
                                 'of the result. (Default is 1)'))
  option_parser.add_option('--test-apk', dest='test_apk',
                           help=('The name of the apk containing the tests '
                                 '(without the .apk extension). For SDK '
                                 'builds, the apk name without the debug '
                                 'suffix(for example, ContentShellTest).'))
  option_parser.add_option('--screenshot', dest='screenshot_failures',
                           action='store_true',
                           help='Capture screenshots of test failures')
  option_parser.add_option('--save-perf-json', action='store_true',
                           help='Saves the JSON file for each UI Perf test.')
  option_parser.add_option('--shard_retries', type=int, default=1,
                           help=('Number of times to retry each failure when '
                                 'sharding.'))
  option_parser.add_option('--official-build', help='Run official build tests.')
  option_parser.add_option('--device',
                           help='Serial number of device we should use.')
  option_parser.add_option('--python_test_root',
                           help='Root of the python-driven tests.')

def ValidateInstrumentationOptions(option_parser, options, args):
  """Validate options/arguments and populate options with defaults."""
  if len(args) > 1:
    option_parser.print_help(sys.stderr)
    option_parser.error('Unknown arguments: %s' % args[1:])
  if options.java_only and options.python_only:
    option_parser.error('Options java_only (-j) and python_only (-p) '
                        'are mutually exclusive.')

  options.run_java_tests = True
  options.run_python_tests = True
  if options.java_only:
    options.run_python_tests = False
  elif options.python_only:
    options.run_java_tests = False

  # In case of SDK Build, the jars and apks have a -debug suffix.
  options.test_apk_path = os.path.join(_SDK_OUT_DIR,
                                       options.build_type,
                                       constants.SDK_BUILD_APKS_DIR,
                                       '%s-debug.apk' % options.test_apk)
  options.test_apk_jar_path = os.path.join(_SDK_OUT_DIR,
                                           options.build_type,
                                           constants.SDK_BUILD_TEST_JAVALIB_DIR,
                                           '%s-debug.jar' % options.test_apk)
  if options.annotation_str:
    options.annotation = options.annotation_str.split()
  elif options.test_filter:
    options.annotation = []
  else:
    options.annotation = ['Smoke', 'SmallTest', 'MediumTest', 'LargeTest']
