# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing information about the python-driven tests."""

import logging
import os

import tests_annotations


class TestInfo(object):
  """An object containing and representing a test function, plus metadata."""

  def __init__(self, runnable, set_up=None, tear_down=None):
    # The actual test function/method.
    self.runnable = runnable
    # Qualified name of test function/method (e.g. FooModule.testBar).
    self.qualified_name = self._GetQualifiedName(runnable)
    # setUp and teardown functions, if any.
    self.set_up = set_up
    self.tear_down = tear_down

  def _GetQualifiedName(self, runnable):
    """Helper method to infer a runnable's name and module name.

    Many filters and lists presuppose a format of module_name.testMethodName.
    To make this easy on everyone, we use some reflection magic to infer this
    name automatically.

    Args:
      runnable: the test method to get the qualified name for

    Returns:
      qualified name for this runnable, incl. module name and method name.
    """
    runnable_name = runnable.__name__
    # See also tests_annotations.
    module_name = os.path.splitext(
        os.path.basename(runnable.__globals__['__file__']))[0]
    return '.'.join([module_name, runnable_name])

  def __str__(self):
    return self.qualified_name


class TestInfoCollection(object):
  """A collection of TestInfo objects which facilitates filtering."""

  def __init__(self):
    """Initialize a new TestInfoCollection."""
    # Master list of all valid tests.
    self.all_tests = []

  def AddTests(self, test_infos):
    """Adds a set of tests to this collection.

    The user may then retrieve them, optionally according to criteria, via
    GetAvailableTests().

    Args:
      test_infos: a list of TestInfos representing test functions/methods.
    """
    self.all_tests = test_infos

  def GetAvailableTests(self, annotation, name_filter):
    """Get a collection of TestInfos which match the supplied criteria.

    Args:
      annotation: annotation which tests must match, if any
      name_filter: name filter which tests must match, if any

    Returns:
      List of available tests.
    """
    available_tests = self.all_tests

    # Filter out tests which match neither the requested annotation, nor the
    # requested name filter, if any.
    available_tests = [t for t in available_tests if
                       self._AnnotationIncludesTest(t, annotation)]
    if annotation and len(annotation) == 1 and annotation[0] == 'SmallTest':
      tests_without_annotation = [
          t for t in self.all_tests if
          not tests_annotations.AnnotatedFunctions.GetTestAnnotations(
              t.qualified_name)]
      test_names = [t.qualified_name for t in tests_without_annotation]
      logging.warning('The following tests do not contain any annotation. '
                      'Assuming "SmallTest":\n%s',
                      '\n'.join(test_names))
      available_tests += tests_without_annotation
    available_tests = [t for t in available_tests if
                       self._NameFilterIncludesTest(t, name_filter)]

    return available_tests

  def _AnnotationIncludesTest(self, test_info, annotation_filter_list):
    """Checks whether a given test represented by test_info matches annotation.

    Args:
      test_info: TestInfo object representing the test
      annotation_filter_list: list of annotation filters to match (e.g. Smoke)

    Returns:
      True if no annotation was supplied or the test matches; false otherwise.
    """
    if not annotation_filter_list:
      return True
    for annotation_filter in annotation_filter_list:
      filters = annotation_filter.split('=')
      if len(filters) == 2:
        key = filters[0]
        value_list = filters[1].split(',')
        for value in value_list:
          if tests_annotations.AnnotatedFunctions.IsAnnotated(
              key + ':' + value, test_info.qualified_name):
            return True
      elif tests_annotations.AnnotatedFunctions.IsAnnotated(
          annotation_filter, test_info.qualified_name):
        return True
    return False

  def _NameFilterIncludesTest(self, test_info, name_filter):
    """Checks whether a name filter matches a given test_info's method name.

    This is a case-sensitive, substring comparison: 'Foo' will match methods
    Foo.testBar and Bar.testFoo. 'foo' would not match either.

    Args:
      test_info: TestInfo object representing the test
      name_filter: substring to check for in the qualified name of the test

    Returns:
      True if no name filter supplied or it matches; False otherwise.
    """
    return not name_filter or name_filter in test_info.qualified_name
