#!/usr/bin/env python
#-*- coding: utf-8 -*-
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Unit test for the build status tracker script."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'


import copy
import unittest

import track_build_status


NORMAL_BOT_TO_STATUS_MAPPING = { '1455--ChromeOS': '455--OK',
                                 '1455--Chrome': '900--failed',
                                 '1455--Linux32DBG': '344--OK',
                                 '1456--ChromeOS': '456--OK' }


class TrackBuildStatusTest(unittest.TestCase):
  def test_that_filter_chrome_only_builds_filter_properly(self):
    bot_to_status_mapping = copy.deepcopy(NORMAL_BOT_TO_STATUS_MAPPING)
    bot_to_status_mapping['133445--Chrome'] = '901--OK'

    result = track_build_status._filter_chrome_only_builds(
        bot_to_status_mapping)

    self.assertEquals(NORMAL_BOT_TO_STATUS_MAPPING, result)

  def test_ensure_filter_chrome_only_builds_doesnt_filter_too_much(self):
    result = track_build_status._filter_chrome_only_builds(
        NORMAL_BOT_TO_STATUS_MAPPING)

    self.assertEquals(NORMAL_BOT_TO_STATUS_MAPPING, result)


if __name__ == '__main__':
  unittest.main()
