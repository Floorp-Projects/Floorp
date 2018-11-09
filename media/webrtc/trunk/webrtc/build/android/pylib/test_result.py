# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import json
import logging
import os
import time
import traceback

import buildbot_report
import constants


class BaseTestResult(object):
  """A single result from a unit test."""

  def __init__(self, name, log):
    self.name = name
    self.log = log.replace('\r', '')


class SingleTestResult(BaseTestResult):
  """Result information for a single test.

  Args:
    full_name: Full name of the test.
    start_date: Date in milliseconds when the test began running.
    dur: Duration of the test run in milliseconds.
    log: An optional string listing any errors.
  """

  def __init__(self, full_name, start_date, dur, log=''):
    BaseTestResult.__init__(self, full_name, log)
    name_pieces = full_name.rsplit('#')
    if len(name_pieces) > 1:
      self.test_name = name_pieces[1]
      self.class_name = name_pieces[0]
    else:
      self.class_name = full_name
      self.test_name = full_name
    self.start_date = start_date
    self.dur = dur


class TestResults(object):
  """Results of a test run."""

  def __init__(self):
    self.ok = []
    self.failed = []
    self.crashed = []
    self.unknown = []
    self.timed_out = False
    self.overall_fail = False

  @staticmethod
  def FromRun(ok=None, failed=None, crashed=None, timed_out=False,
              overall_fail=False):
    ret = TestResults()
    ret.ok = ok or []
    ret.failed = failed or []
    ret.crashed = crashed or []
    ret.timed_out = timed_out
    ret.overall_fail = overall_fail
    return ret

  @staticmethod
  def FromTestResults(results):
    """Combines a list of results in a single TestResults object."""
    ret = TestResults()
    for t in results:
      ret.ok += t.ok
      ret.failed += t.failed
      ret.crashed += t.crashed
      ret.unknown += t.unknown
      if t.timed_out:
        ret.timed_out = True
      if t.overall_fail:
        ret.overall_fail = True
    return ret

  @staticmethod
  def FromPythonException(test_name, start_date_ms, exc_info):
    """Constructs a TestResults with exception information for the given test.

    Args:
      test_name: name of the test which raised an exception.
      start_date_ms: the starting time for the test.
      exc_info: exception info, ostensibly from sys.exc_info().

    Returns:
      A TestResults object with a SingleTestResult in the failed list.
    """
    exc_type, exc_value, exc_traceback = exc_info
    trace_info = ''.join(traceback.format_exception(exc_type, exc_value,
                                                    exc_traceback))
    log_msg = 'Exception:\n' + trace_info
    duration_ms = (int(time.time()) * 1000) - start_date_ms

    exc_result = SingleTestResult(
                     full_name='PythonWrapper#' + test_name,
                     start_date=start_date_ms,
                     dur=duration_ms,
                     log=(str(exc_type) + ' ' + log_msg))

    results = TestResults()
    results.failed.append(exc_result)
    return results

  def _Log(self, sorted_list):
    for t in sorted_list:
      logging.critical(t.name)
      if t.log:
        logging.critical(t.log)

  def GetAllBroken(self):
    """Returns the all broken tests including failed, crashed, unknown."""
    return self.failed + self.crashed + self.unknown

  def LogFull(self, test_group, test_suite, build_type):
    """Output broken test logs, summarize in a log file and the test output."""
    # Output all broken tests or 'passed' if none broken.
    logging.critical('*' * 80)
    logging.critical('Final result')
    if self.failed:
      logging.critical('Failed:')
      self._Log(sorted(self.failed))
    if self.crashed:
      logging.critical('Crashed:')
      self._Log(sorted(self.crashed))
    if self.unknown:
      logging.critical('Unknown:')
      self._Log(sorted(self.unknown))
    if not self.GetAllBroken():
      logging.critical('Passed')
    logging.critical('*' * 80)

    # Summarize in a log file, if tests are running on bots.
    if test_group and test_suite and os.environ.get('BUILDBOT_BUILDERNAME'):
      log_file_path = os.path.join(constants.CHROME_DIR, 'out',
                                   build_type, 'test_logs')
      if not os.path.exists(log_file_path):
        os.mkdir(log_file_path)
      full_file_name = os.path.join(log_file_path, test_group)
      if not os.path.exists(full_file_name):
        with open(full_file_name, 'w') as log_file:
          print >> log_file, '\n%s results for %s build %s:' % (
              test_group, os.environ.get('BUILDBOT_BUILDERNAME'),
              os.environ.get('BUILDBOT_BUILDNUMBER'))
      log_contents = ['  %s result : %d tests ran' % (test_suite,
                                                      len(self.ok) +
                                                      len(self.failed) +
                                                      len(self.crashed) +
                                                      len(self.unknown))]
      content_pairs = [('passed', len(self.ok)), ('failed', len(self.failed)),
                       ('crashed', len(self.crashed))]
      for (result, count) in content_pairs:
        if count:
          log_contents.append(', %d tests %s' % (count, result))
      with open(full_file_name, 'a') as log_file:
        print >> log_file, ''.join(log_contents)
      content = {'test_group': test_group,
                 'ok': [t.name for t in self.ok],
                 'failed': [t.name for t in self.failed],
                 'crashed': [t.name for t in self.failed],
                 'unknown': [t.name for t in self.unknown],}
      with open(os.path.join(log_file_path, 'results.json'), 'a') as json_file:
        print >> json_file, json.dumps(content)

    # Summarize in the test output.
    summary_string = 'Summary:\n'
    summary_string += 'RAN=%d\n' % (len(self.ok) + len(self.failed) +
                                    len(self.crashed) + len(self.unknown))
    summary_string += 'PASSED=%d\n' % (len(self.ok))
    summary_string += 'FAILED=%d %s\n' % (len(self.failed),
                                          [t.name for t in self.failed])
    summary_string += 'CRASHED=%d %s\n' % (len(self.crashed),
                                           [t.name for t in self.crashed])
    summary_string += 'UNKNOWN=%d %s\n' % (len(self.unknown),
                                           [t.name for t in self.unknown])
    logging.critical(summary_string)
    return summary_string

  def PrintAnnotation(self):
    """Print buildbot annotations for test results."""
    if self.timed_out:
      buildbot_report.PrintWarning()
    elif self.failed or self.crashed or self.overall_fail:
      buildbot_report.PrintError()
    else:
      print 'Step success!'  # No annotation needed
