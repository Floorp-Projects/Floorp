#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Unit tests for helper functions in utils.py."""

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

import unittest

from webrtc_buildbot import utils


class Test(unittest.TestCase):

  def testGetEnabledTests(self):
    tests = {
        # Name     Linux Mac   Windows
        'test_1': (True, True, False),
        'test_2': (True, False, False),
    }
    result = utils.GetEnabledTests(tests, 'Linux')
    self.assertEqual(2, len(result))
    self.assertEqual('test_1', result[0])
    self.assertEqual('test_2', result[1])

    result = utils.GetEnabledTests(tests, 'Mac')
    self.assertEqual(1, len(result))
    self.assertEqual('test_1', result[0])

    result = utils.GetEnabledTests(tests, 'Windows')
    self.assertEqual(0, len(result))

    self.assertRaises(utils.UnsupportedPlatformError,
                      utils.GetEnabledTests, tests, 'BeOS')

  def testEmptyListExitQuietly(self):
    factory = utils.WebRTCFactory()
    self.assertEqual([], factory._WrapLongLines([]))

  def testShortLinesShallNotWrap(self):
    factory = utils.WebRTCFactory()
    self.assertEqual(['a'], factory._WrapLongLines(['a']))

    string_25_len = '12345678901234567890123_5'
    result = factory._WrapLongLines([string_25_len, string_25_len])
    self.assertEqual(string_25_len, result[0])
    self.assertEqual(string_25_len, result[1])

  def testLinesWithMoreThan25CharactersWithNoWrapCharacterIsUnchanged(self):
    factory = utils.WebRTCFactory()
    string_26_len = '12345678901234567890123456'
    result = factory._WrapLongLines([string_26_len, string_26_len])
    self.assertEqual(string_26_len, result[0])
    self.assertEqual(string_26_len, result[1])

  def testLinesWithMoreThan25CharactersShallWrapOnWrapCharacter(self):
    factory = utils.WebRTCFactory()
    string_26_len = '123456789012345678901234_6'
    test_list = [string_26_len, string_26_len]
    result = factory._WrapLongLines(test_list)
    expected_result = '123456789012345678901234 _6'
    self.assertEqual(expected_result, result[0])
    self.assertEqual(expected_result, result[1])
    # Verify the original test_list was not modified too.
    self.assertEqual(string_26_len, test_list[0])
    self.assertEqual(string_26_len, test_list[1])


if __name__ == '__main__':
  unittest.main()
