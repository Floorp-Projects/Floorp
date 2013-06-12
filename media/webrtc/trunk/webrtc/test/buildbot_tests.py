#!/usr/bin/env python
#
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


"""Script to run tests with pre-configured command line arguments.

NOTICE: This test is designed to be run from the build output folder! It is
copied automatically during build.

With this script, it's easier for anyone to enable/disable or extend a test that
runs on the buildbots. It is also easier for developers to run the tests in the
same way as they run on the bots.
"""

import optparse
import os
import subprocess
import sys

_CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
_HOME = os.environ.get('HOME', '')

_VIE_AUTO_TEST_CMD_LIST = [
    'vie_auto_test',
    '--automated',
    '--gtest_filter=-ViERtpFuzzTest*',
    '--capture_test_ensure_resolution_alignment_in_capture_device=false']
_WIN_TESTS = {
    'vie_auto_test': _VIE_AUTO_TEST_CMD_LIST,
    'voe_auto_test': ['voe_auto_test',
                      '--automated'],
}
_MAC_TESTS = {
    'vie_auto_test': _VIE_AUTO_TEST_CMD_LIST,
    'voe_auto_test': ['voe_auto_test',
                      '--automated',
                      ('--gtest_filter='
                       '-VolumeTest.SetVolumeBeforePlayoutWorks' # bug 527
                       )],
}
_LINUX_TESTS = {
    'vie_auto_test': _VIE_AUTO_TEST_CMD_LIST,
    'voe_auto_test': ['voe_auto_test',
                      '--automated',
                      '--gtest_filter=-RtpFuzzTest.*'],
    'audio_e2e_test': ['python',
                       'run_audio_test.py',
                       '--input=../../resources/e2e_audio_in.pcm',
                       '--output=/tmp/e2e_audio_out.pcm',
                       '--codec=L16',
                       '--harness=%s/audio_e2e_harness' % _CURRENT_DIR,
                       '--compare=%s/bin/compare-audio +16000 +wb' % _HOME,
                       '--regexp=(\d\.\d{3})'],
    'audioproc_perf': ['audioproc',
                       '-aecm', '-ns', '-agc', '--fixed_digital', '--perf',
                       '-pb', '../../resources/audioproc.aecdump'],
    'isac_fixed_perf': ['iSACFixtest',
                        '32000', '../../resources/speech_and_misc_wb.pcm',
                        'isac_speech_and_misc_wb.pcm'],
}


def main():
  parser = optparse.OptionParser('usage: %prog -t <test> [-t <test> ...]\n'
                                 'If no test is specified, all tests are run.')
  parser.add_option('-l', '--list', action='store_true', default=False,
                    help='Lists all currently supported tests.')
  parser.add_option('-t', '--test', action='append', default=[],
                    help='Which test to run. May be specified multiple times.')
  options, _ = parser.parse_args()

  if sys.platform.startswith('win'):
    test_dict = _WIN_TESTS
  elif sys.platform.startswith('linux'):
    test_dict = _LINUX_TESTS
  elif sys.platform.startswith('darwin'):
    test_dict = _MAC_TESTS
  else:
    parser.error('Unsupported platform: %s' % sys.platform)

  if options.list:
    print 'Available tests:'
    print 'Test name              Command line'
    print '=========              ============'
    for test, cmd_line in test_dict.items():
      print '%-20s   %s' % (test, ' '.join(cmd_line))
    return

  if not options.test:
    options.test = test_dict.keys()
  for test in options.test:
    if test not in test_dict:
      parser.error('Test "%s" is not supported (use --list to view supported '
                   'tests).')

  # Change current working directory to the script's dir to make the relative
  # paths always work.
  os.chdir(_CURRENT_DIR)
  print 'Changed working directory to: %s' % _CURRENT_DIR

  print 'Running WebRTC Buildbot tests: %s' % options.test
  for test in options.test:
    cmd_line = test_dict[test]

    # Create absolute paths to test executables for non-Python tests.
    if cmd_line[0] != 'python':
      cmd_line[0] = os.path.join(_CURRENT_DIR, cmd_line[0])

    print 'Running: %s' % ' '.join(cmd_line)
    try:
      subprocess.check_call(cmd_line)
    except subprocess.CalledProcessError as e:
      print >> sys.stderr, ('An error occurred during test execution: return '
                            'code: %d' % e.returncode)
      return -1

  print 'Testing completed successfully.'
  return 0


if __name__ == '__main__':
  sys.exit(main())
