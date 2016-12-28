#!/usr/bin/env python
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
Runs tests on Android devices.

This script exists to avoid WebRTC being broken by changes in the Chrome Android
test execution toolchain. It also conveniently sets the CHECKOUT_SOURCE_ROOT
environment variable.
"""

import os
import sys

SCRIPT_DIR = os.path.dirname(__file__)
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir,
                                        os.pardir))
CHROMIUM_BUILD_ANDROID_DIR = os.path.join(SRC_DIR, 'build', 'android')
sys.path.insert(0, CHROMIUM_BUILD_ANDROID_DIR)


import test_runner  # pylint: disable=W0406
from pylib.gtest import gtest_test_instance

def main():
  # Set our own paths to the .isolate files.
  # pylint: disable=protected-access
  gtest_test_instance._DEFAULT_ISOLATE_FILE_PATHS.update({
    'audio_decoder_unittests':
        'webrtc/modules/audio_decoder_unittests.isolate',
    'common_audio_unittests':
        'webrtc/common_audio/common_audio_unittests.isolate',
    'common_video_unittests':
        'webrtc/common_video/common_video_unittests.isolate',
    'libjingle_peerconnection_unittest':
        'talk/libjingle_peerconnection_unittest.isolate',
    'modules_tests': 'webrtc/modules/modules_tests.isolate',
    'modules_unittests': 'webrtc/modules/modules_unittests.isolate',
    'rtc_unittests': 'webrtc/rtc_unittests.isolate',
    'system_wrappers_unittests':
        'webrtc/system_wrappers/system_wrappers_unittests.isolate',
    'test_support_unittests': 'webrtc/test/test_support_unittests.isolate',
    'tools_unittests': 'webrtc/tools/tools_unittests.isolate',
    'video_capture_tests':
        'webrtc/modules/video_capture/video_capture_tests.isolate',
    'video_engine_tests': 'webrtc/video_engine_tests.isolate',
    'voice_engine_unittests':
        'webrtc/voice_engine/voice_engine_unittests.isolate',
    'webrtc_nonparallel_tests': 'webrtc/webrtc_nonparallel_tests.isolate',
    'webrtc_perf_tests': 'webrtc/webrtc_perf_tests.isolate',
  })
  # Override environment variable to make it possible for the scripts to find
  # the root directory (our symlinking of the Chromium build toolchain would
  # otherwise make them fail to do so).
  os.environ['CHECKOUT_SOURCE_ROOT'] = SRC_DIR
  return test_runner.main()

if __name__ == '__main__':
  sys.exit(main())
