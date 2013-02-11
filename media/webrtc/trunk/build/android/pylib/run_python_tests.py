# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the Python tests (relies on using the Java test runner)."""

import logging
import os
import sys
import types

import android_commands
import apk_info
import constants
import python_test_base
from python_test_caller import CallPythonTest
from python_test_sharder import PythonTestSharder
import run_java_tests
from run_java_tests import FatalTestException
from test_info_collection import TestInfoCollection
from test_result import TestResults


def _GetPythonFiles(root, files):
  """Returns all files from |files| that end in 'Test.py'.

  Args:
    root: A directory name with python files.
    files: A list of file names.

  Returns:
    A list with all Python driven test file paths.
  """
  return [os.path.join(root, f) for f in files if f.endswith('Test.py')]


def _InferImportNameFromFile(python_file):
  """Given a file, infer the import name for that file.

  Example: /usr/foo/bar/baz.py -> baz.

  Args:
    python_file: path to the Python file, ostensibly to import later.

  Returns:
    The module name for the given file.
  """
  return os.path.splitext(os.path.basename(python_file))[0]


def DispatchPythonTests(options):
  """Dispatches the Python tests. If there are multiple devices, use sharding.

  Args:
    options: command line options.

  Returns:
    A list of test results.
  """

  attached_devices = android_commands.GetAttachedDevices()
  if not attached_devices:
    raise FatalTestException('You have no devices attached or visible!')
  if options.device:
    attached_devices = [options.device]

  test_collection = TestInfoCollection()
  all_tests = _GetAllTests(options.python_test_root, options.official_build)
  test_collection.AddTests(all_tests)
  test_names = [t.qualified_name for t in all_tests]
  logging.debug('All available tests: ' + str(test_names))

  available_tests = test_collection.GetAvailableTests(
      options.annotation, options.test_filter)

  if not available_tests:
    logging.warning('No Python tests to run with current args.')
    return TestResults()

  available_tests *= options.number_of_runs
  test_names = [t.qualified_name for t in available_tests]
  logging.debug('Final list of tests to run: ' + str(test_names))

  # Copy files to each device before running any tests.
  for device_id in attached_devices:
    logging.debug('Pushing files to device %s', device_id)
    apks = [apk_info.ApkInfo(options.test_apk_path, options.test_apk_jar_path)]
    test_files_copier = run_java_tests.TestRunner(options, device_id,
                                                  None, False, 0, apks, [])
    test_files_copier.CopyTestFilesOnce()

  # Actually run the tests.
  if len(attached_devices) > 1 and options.wait_for_debugger:
    logging.warning('Debugger can not be sharded, '
                    'using first available device')
    attached_devices = attached_devices[:1]
  logging.debug('Running Python tests')
  sharder = PythonTestSharder(attached_devices, available_tests, options)
  test_results = sharder.RunShardedTests()

  return test_results


def _GetTestModules(python_test_root, is_official_build):
  """Retrieve a sorted list of pythonDrivenTests.

  Walks the location of pythonDrivenTests, imports them, and provides the list
  of imported modules to the caller.

  Args:
    python_test_root: the path to walk, looking for pythonDrivenTests
    is_official_build: whether to run only those tests marked 'official'

  Returns:
    A list of Python modules which may have zero or more tests.
  """
  # By default run all python tests under pythonDrivenTests.
  python_test_file_list = []
  for root, _, files in os.walk(python_test_root):
    if (root.endswith('pythonDrivenTests')
        or (is_official_build
            and root.endswith('pythonDrivenTests/official'))):
      python_test_file_list += _GetPythonFiles(root, files)
  python_test_file_list.sort()

  test_module_list = [_GetModuleFromFile(test_file)
                      for test_file in python_test_file_list]
  return test_module_list


def _GetModuleFromFile(python_file):
  """Gets the module associated with a file by importing it.

  Args:
    python_file: file to import

  Returns:
    The module object.
  """
  sys.path.append(os.path.dirname(python_file))
  import_name = _InferImportNameFromFile(python_file)
  return __import__(import_name)


def _GetTestsFromClass(test_class):
  """Create a list of test objects for each test method on this class.

  Test methods are methods on the class which begin with 'test'.

  Args:
    test_class: class object which contains zero or more test methods.

  Returns:
    A list of test objects, each of which is bound to one test.
  """
  test_names = [m for m in dir(test_class)
                if _IsTestMethod(m, test_class)]
  return map(test_class, test_names)


def _GetTestClassesFromModule(test_module):
  tests = []
  for name in dir(test_module):
    attr = getattr(test_module, name)
    if _IsTestClass(attr):
      tests.extend(_GetTestsFromClass(attr))
  return tests


def _IsTestClass(test_class):
  return (type(test_class) is types.TypeType and
          issubclass(test_class, python_test_base.PythonTestBase) and
          test_class is not python_test_base.PythonTestBase)


def _IsTestMethod(attrname, test_case_class):
  """Checks whether this is a valid test method.

  Args:
    attrname: the method name.
    test_case_class: the test case class.

  Returns:
    True if test_case_class.'attrname' is callable and it starts with 'test';
    False otherwise.
  """
  attr = getattr(test_case_class, attrname)
  return callable(attr) and attrname.startswith('test')


def _GetAllTests(test_root, is_official_build):
  """Retrieve a list of Python test modules and their respective methods.

  Args:
    test_root: path which contains Python-driven test files
    is_official_build: whether this is an official build

  Returns:
    List of test case objects for all available test methods.
  """
  if not test_root:
    return []
  all_tests = []
  test_module_list = _GetTestModules(test_root, is_official_build)
  for module in test_module_list:
    all_tests.extend(_GetTestClassesFromModule(module))
  return all_tests
