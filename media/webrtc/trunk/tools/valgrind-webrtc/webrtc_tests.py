#!/usr/bin/env python
# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

''' Runs various WebRTC tests through valgrind_test.py.

This script inherits the chrome_tests.py in Chrome, replacing its tests with
our own in WebRTC instead.
'''

import optparse
import sys

import logging_utils

import chrome_tests

class WebRTCTests(chrome_tests.ChromeTests):
  # WebRTC tests, similar functions for each tests as the Chrome tests in the
  # parent class.
  def TestSignalProcessing(self):
    return self.SimpleTest("signal_processing", "signal_processing_unittests")

  def TestResampler(self):
    return self.SimpleTest("resampler", "resampler_unittests")

  def TestVAD(self):
    return self.SimpleTest("vad", "vad_unittests")

  def TestCNG(self):
    return self.SimpleTest("cng", "cng_unittests")

  def TestG711(self):
    return self.SimpleTest("g711", "g711_unittests")

  def TestG722(self):
    return self.SimpleTest("g722", "g722_unittests")

  def TestPCM16B(self):
    return self.SimpleTest("pcm16b", "pcm16b_unittests")

  def TestNetEQ(self):
    return self.SimpleTest("neteq", "neteq_unittests")

  def TestAudioConferenceMixer(self):
    return self.SimpleTest("audio_conference_mixer", "audio_conference_mixer_unittests")

  def TestMediaFile(self):
    return self.SimpleTest("media_file", "media_file_unittests")

  def TestRTPRTCP(self):
    return self.SimpleTest("rtp_rtcp", "rtp_rtcp_unittests")

  def TestBWE(self):
    return self.SimpleTest("test_bwe", "test_bwe")

  def TestUDPTransport(self):
    return self.SimpleTest("udp_transport", "udp_transport_unittests")

  def TestWebRTCUtility(self):
    return self.SimpleTest("webrtc_utility", "webrtc_utility_unittests")

  def TestVP8(self):
    return self.SimpleTest("vp8", "vp8_unittests")

  def TestVideoCoding(self):
    return self.SimpleTest("video_coding", "video_coding_unittests")

  def TestVideoProcessing(self):
    return self.SimpleTest("video_processing", "video_processing_unittests")

  def TestSystemWrappers(self):
    return self.SimpleTest("system_wrappers", "system_wrappers_unittests")

  def TestTestSupport(self):
    return self.SimpleTest("test_support", "test_support_unittests")

def _main(_):
  parser = optparse.OptionParser("usage: %prog -b <dir> -t <test> "
                                 "[-t <test> ...]")
  parser.disable_interspersed_args()
  parser.add_option("-b", "--build_dir",
                    help="the location of the compiler output")
  parser.add_option("-t", "--test", action="append", default=[],
                    help="which test to run, supports test:gtest_filter format "
                         "as well.")
  parser.add_option("", "--baseline", action="store_true", default=False,
                    help="generate baseline data instead of validating")
  parser.add_option("", "--gtest_filter",
                    help="additional arguments to --gtest_filter")
  parser.add_option("", "--gtest_repeat",
                    help="argument for --gtest_repeat")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
  parser.add_option("", "--tool", dest="valgrind_tool", default="memcheck",
                    help="specify a valgrind tool to run the tests under")
  parser.add_option("", "--tool_flags", dest="valgrind_tool_flags", default="",
                    help="specify custom flags for the selected valgrind tool")
  parser.add_option("", "--keep_logs", action="store_true", default=False,
                    help="store memory tool logs in the <tool>.logs directory "
                         "instead of /tmp.\nThis can be useful for tool "
                         "developers/maintainers.\nPlease note that the <tool>"
                         ".logs directory will be clobbered on tool startup.")
  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if not options.test:
    parser.error("--test not specified")

  if len(options.test) != 1 and options.gtest_filter:
    parser.error("--gtest_filter and multiple tests don't make sense together")

  for t in options.test:
    tests = WebRTCTests(options, args, t)
    ret = tests.Run()
    if ret: return ret
  return 0

if __name__ == "__main__":
  # Overwrite the ChromeTests tests dictionary with our WebRTC tests. 
  # The cmdline option allows the user to pass any executable as parameter to
  # the test script, which is useful when developing new tests that are not yet
  # present in this script.
  chrome_tests.ChromeTests._test_list = {
    "cmdline": chrome_tests.ChromeTests.RunCmdLine,
    "signal_processing": WebRTCTests.TestSignalProcessing,
    "resampler": WebRTCTests.TestResampler,
    "vad": WebRTCTests.TestVAD,
    "cng": WebRTCTests.TestCNG,
    "g711": WebRTCTests.TestG711,
    "g722": WebRTCTests.TestG722,
    "pcm16b": WebRTCTests.TestPCM16B,
    "neteq": WebRTCTests.TestNetEQ,
    "audio_conference_mixer": WebRTCTests.TestAudioConferenceMixer,
    "media_file": WebRTCTests.TestMediaFile,
    "rtp_rtcp": WebRTCTests.TestRTPRTCP,
    "test_bwe": WebRTCTests.TestBWE,
    "udp_transport": WebRTCTests.TestUDPTransport,
    "webrtc_utility": WebRTCTests.TestWebRTCUtility,
    "vp8": WebRTCTests.TestVP8,
    "video_coding": WebRTCTests.TestVideoCoding,
    "video_processing": WebRTCTests.TestVideoProcessing,
    "system_wrappers": WebRTCTests.TestSystemWrappers,
    "test_support": WebRTCTests.TestTestSupport,
  }
  ret = _main(sys.argv)
  sys.exit(ret)  