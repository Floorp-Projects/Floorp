# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'targets': [
    {
      'target_name': 'rtc_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'base/base.gyp:rtc_base',
        'base/base_tests.gyp:rtc_base_tests_utils',
        'base/base_tests.gyp:rtc_base_tests',
        'libjingle/xmllite/xmllite.gyp:rtc_xmllite',
        'libjingle/xmpp/xmpp.gyp:rtc_xmpp',
        'p2p/p2p.gyp:rtc_p2p',
        'p2p/p2p.gyp:libstunprober',
        'rtc_p2p_unittest',
        'rtc_sound_tests',
        'rtc_xmllite_unittest',
        'rtc_xmpp_unittest',
        'sound/sound.gyp:rtc_sound',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['OS=="ios"', {
          'dependencies': [
            'api/api_tests.gyp:rtc_api_objc_test',
          ]
        }]
      ],
    },
    {
      'target_name': 'webrtc_tests',
      'type': 'none',
      'dependencies': [
        'video_engine_tests',
        'video_loopback',
        'video_replay',
        'webrtc_perf_tests',
        'webrtc_nonparallel_tests',
      ],
    },
    {
      'target_name': 'video_quality_test',
      'type': 'static_library',
      'sources': [
        'video/video_quality_test.cc',
        'video/video_quality_test.h',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/modules/modules.gyp:video_render',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        'webrtc',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies!': [
            '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
          ],
        }],
      ],
    },
    {
      'target_name': 'video_loopback',
      'type': 'executable',
      'sources': [
        'test/mac/run_test.mm',
        'test/run_test.cc',
        'test/run_test.h',
        'video/video_loopback.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'test/run_test.cc',
          ],
        }],
      ],
      'dependencies': [
        'video_quality_test',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/webrtc_test_common.gyp:webrtc_test_renderer',
        'test/test.gyp:test_main',
        'webrtc',
      ],
    },
    {
      'target_name': 'screenshare_loopback',
      'type': 'executable',
      'sources': [
        'test/mac/run_test.mm',
        'test/run_test.cc',
        'test/run_test.h',
        'video/screenshare_loopback.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'test/run_test.cc',
          ],
        }],
      ],
      'dependencies': [
        'video_quality_test',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/webrtc_test_common.gyp:webrtc_test_renderer',
        'test/test.gyp:test_main',
        'webrtc',
      ],
    },
    {
      'target_name': 'video_replay',
      'type': 'executable',
      'sources': [
        'test/mac/run_test.mm',
        'test/run_test.cc',
        'test/run_test.h',
        'video/replay.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            'test/run_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/webrtc_test_common.gyp:webrtc_test_renderer',
        '<(webrtc_root)/modules/modules.gyp:video_capture',
        '<(webrtc_root)/modules/modules.gyp:video_render',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
        'webrtc',
      ],
    },
    {
      # TODO(solenberg): Rename to webrtc_call_tests.
      'target_name': 'video_engine_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'audio/audio_receive_stream_unittest.cc',
        'audio/audio_send_stream_unittest.cc',
        'audio/audio_state_unittest.cc',
        'call/bitrate_allocator_unittest.cc',
        'call/bitrate_estimator_tests.cc',
        'call/call_unittest.cc',
        'call/packet_injection_tests.cc',
        'test/common_unittest.cc',
        'test/testsupport/metrics/video_metrics_unittest.cc',
        'video/call_stats_unittest.cc',
        'video/encoder_state_feedback_unittest.cc',
        'video/end_to_end_tests.cc',
        'video/overuse_frame_detector_unittest.cc',
        'video/payload_router_unittest.cc',
        'video/report_block_stats_unittest.cc',
        'video/send_statistics_proxy_unittest.cc',
        'video/stream_synchronization_unittest.cc',
        'video/video_capture_input_unittest.cc',
        'video/video_decoder_unittest.cc',
        'video/video_encoder_unittest.cc',
        'video/video_send_stream_tests.cc',
        'video/vie_codec_unittest.cc',
        'video/vie_remb_unittest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
        '<(webrtc_root)/modules/modules.gyp:video_capture',
        '<(webrtc_root)/modules/modules.gyp:video_render',
        '<(webrtc_root)/test/test.gyp:channel_transport',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
        'test/metrics.gyp:metrics',
        'test/test.gyp:test_main',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'webrtc',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['enable_protobuf==1', {
          'defines': [
            'ENABLE_RTC_EVENT_LOG',
          ],
          'dependencies': [
            'webrtc.gyp:rtc_event_log',
            'webrtc.gyp:rtc_event_log_proto',
          ],
          'sources': [
            'call/rtc_event_log_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'webrtc_perf_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'call/call_perf_tests.cc',
        'call/rampup_tests.cc',
        'call/rampup_tests.h',
        'modules/audio_coding/neteq/test/neteq_performance_unittest.cc',
        'modules/audio_processing/audio_processing_performance_unittest.cc',
        'modules/remote_bitrate_estimator/remote_bitrate_estimators_test.cc',
        'video/full_stack.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/modules/modules.gyp:audio_processing',
        '<(webrtc_root)/modules/modules.gyp:audioproc_test_utils',
        '<(webrtc_root)/modules/modules.gyp:video_capture',
        '<(webrtc_root)/test/test.gyp:channel_transport',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
        'video_quality_test',
        'modules/modules.gyp:neteq_test_support',
        'modules/modules.gyp:bwe_simulator',
        'modules/modules.gyp:rtp_rtcp',
        'test/test.gyp:test_main',
        'test/webrtc_test_common.gyp:webrtc_test_common',
        'test/webrtc_test_common.gyp:webrtc_test_renderer',
        'webrtc',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
    {
      'target_name': 'webrtc_nonparallel_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'base/nullsocketserver_unittest.cc',
        'base/physicalsocketserver_unittest.cc',
        'base/socket_unittest.cc',
        'base/socket_unittest.h',
        'base/socketaddress_unittest.cc',
        'base/virtualsocket_unittest.cc',
      ],
      'defines': [
        'GTEST_RELATIVE_PATH',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'base/base.gyp:rtc_base',
        'test/test.gyp:test_main',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'base/win32socketserver_unittest.cc',
          ],
          'sources!': [
            # TODO(ronghuawu): Fix TestUdpReadyToSendIPv6 on windows bot
            # then reenable these tests.
            # TODO(pbos): Move test disabling to ifdefs within the test files
            # instead of here.
            'base/physicalsocketserver_unittest.cc',
            'base/socket_unittest.cc',
            'base/win32socketserver_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'base/macsocketserver_unittest.cc',
          ],
        }],
        ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
          'defines': [
            'CARBON_DEPRECATED=YES',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'rtc_unittests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):rtc_unittests_apk',
          ],
        },
        {
          'target_name': 'video_engine_tests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):video_engine_tests_apk',
          ],
        },
        {
          'target_name': 'webrtc_perf_tests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):webrtc_perf_tests_apk',
          ],
        },
        {
          'target_name': 'webrtc_nonparallel_tests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):webrtc_nonparallel_tests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'rtc_unittests_run',
          'type': 'none',
          'dependencies': [
            'rtc_unittests',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'rtc_unittests.isolate',
          ],
        },
        {
          'target_name': 'video_engine_tests_run',
          'type': 'none',
          'dependencies': [
            'video_engine_tests',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'video_engine_tests.isolate',
          ],
        },
        {
          'target_name': 'webrtc_nonparallel_tests_run',
          'type': 'none',
          'dependencies': [
            'webrtc_nonparallel_tests',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'webrtc_nonparallel_tests.isolate',
          ],
        },
        {
          'target_name': 'webrtc_perf_tests_run',
          'type': 'none',
          'dependencies': [
            'webrtc_perf_tests',
          ],
          'includes': [
            'build/isolate.gypi',
          ],
          'sources': [
            'webrtc_perf_tests.isolate',
          ],
        },
      ],
    }],
  ],
}
