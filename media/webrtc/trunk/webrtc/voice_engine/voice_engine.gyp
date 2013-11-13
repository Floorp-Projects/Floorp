# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'voice_engine',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/modules/modules.gyp:audio_coding_module',
        '<(webrtc_root)/modules/modules.gyp:audio_conference_mixer',
        '<(webrtc_root)/modules/modules.gyp:audio_device',
        '<(webrtc_root)/modules/modules.gyp:audio_processing',
        '<(webrtc_root)/modules/modules.gyp:media_file',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
        '<(webrtc_root)/modules/modules.gyp:webrtc_utility',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'defines': [
        'WEBRTC_EXTERNAL_TRANSPORT',
      ],
      'sources': [
        '../common_types.h',
        '../engine_configurations.h',
        '../typedefs.h',
        'include/voe_audio_processing.h',
        'include/voe_base.h',
        'include/voe_call_report.h',
        'include/voe_codec.h',
        'include/voe_dtmf.h',
        'include/voe_encryption.h',
        'include/voe_errors.h',
        'include/voe_external_media.h',
        'include/voe_file.h',
        'include/voe_hardware.h',
        'include/voe_neteq_stats.h',
        'include/voe_network.h',
        'include/voe_rtp_rtcp.h',
        'include/voe_video_sync.h',
        'include/voe_volume_control.h',
        'channel.cc',
        'channel.h',
        'channel_manager.cc',
        'channel_manager.h',
        'dtmf_inband.cc',
        'dtmf_inband.h',
        'dtmf_inband_queue.cc',
        'dtmf_inband_queue.h',
        'level_indicator.cc',
        'level_indicator.h',
        'monitor_module.cc',
        'monitor_module.h',
        'output_mixer.cc',
        'output_mixer.h',
        'output_mixer_internal.cc',
        'output_mixer_internal.h',
        'shared_data.cc',
        'shared_data.h',
        'statistics.cc',
        'statistics.h',
        'transmit_mixer.cc',
        'transmit_mixer.h',
        'utility.cc',
        'utility.h',
        'voe_audio_processing_impl.cc',
        'voe_audio_processing_impl.h',
        'voe_base_impl.cc',
        'voe_base_impl.h',
        'voe_call_report_impl.cc',
        'voe_call_report_impl.h',
        'voe_codec_impl.cc',
        'voe_codec_impl.h',
        'voe_dtmf_impl.cc',
        'voe_dtmf_impl.h',
        'voe_encryption_impl.cc',
        'voe_encryption_impl.h',
        'voe_external_media_impl.cc',
        'voe_external_media_impl.h',
        'voe_file_impl.cc',
        'voe_file_impl.h',
        'voe_hardware_impl.cc',
        'voe_hardware_impl.h',
        'voe_neteq_stats_impl.cc',
        'voe_neteq_stats_impl.h',
        'voe_network_impl.cc',
        'voe_network_impl.h',
        'voe_rtp_rtcp_impl.cc',
        'voe_rtp_rtcp_impl.h',
        'voe_video_sync_impl.cc',
        'voe_video_sync_impl.h',
        'voe_volume_control_impl.cc',
        'voe_volume_control_impl.h',
        'voice_engine_defines.h',
        'voice_engine_impl.cc',
        'voice_engine_impl.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'defines': ['WEBRTC_DRIFT_COMPENSATION_SUPPORTED',],
    }],
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'voice_engine_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'voice_engine',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            # The rest are to satisfy the unittests' include chain.
            # This would be unnecessary if we used qualified includes.
            '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
            '<(webrtc_root)/modules/modules.gyp:audio_device',
            '<(webrtc_root)/modules/modules.gyp:audio_processing',
            '<(webrtc_root)/modules/modules.gyp:audio_coding_module',
            '<(webrtc_root)/modules/modules.gyp:audio_conference_mixer',
            '<(webrtc_root)/modules/modules.gyp:media_file',
            '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
            '<(webrtc_root)/modules/modules.gyp:webrtc_utility',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'channel_unittest.cc',
            'output_mixer_unittest.cc',
            'transmit_mixer_unittest.cc',
            'voe_audio_processing_unittest.cc',
            'voe_base_unittest.cc',
            'voe_codec_unittest.cc',
          ],
          'conditions': [
            # TODO(henrike): remove build_with_chromium==1 when the bots are
            # using Chromium's buildbots.
            ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
              'dependencies': [
                '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        },
        {
          'target_name': 'voe_auto_test',
          'type': 'executable',
          'dependencies': [
            'voice_engine',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/libtest/libtest.gyp:libtest',
            '<(webrtc_root)/test/test.gyp:channel_transport',
            '<(webrtc_root)/test/test.gyp:test_support',
           ],
          'sources': [
            'test/auto_test/automated_mode.cc',
            'test/auto_test/extended/agc_config_test.cc',
            'test/auto_test/extended/ec_metrics_test.cc',
            'test/auto_test/fakes/fake_external_transport.cc',
            'test/auto_test/fakes/fake_external_transport.h',
            'test/auto_test/fixtures/after_initialization_fixture.cc',
            'test/auto_test/fixtures/after_initialization_fixture.h',
            'test/auto_test/fixtures/after_streaming_fixture.cc',
            'test/auto_test/fixtures/after_streaming_fixture.h',
            'test/auto_test/fixtures/before_initialization_fixture.cc',
            'test/auto_test/fixtures/before_initialization_fixture.h',
            'test/auto_test/fuzz/rtp_fuzz_test.cc',
            'test/auto_test/standard/audio_processing_test.cc',
            'test/auto_test/standard/call_report_test.cc',
            'test/auto_test/standard/codec_before_streaming_test.cc',
            'test/auto_test/standard/codec_test.cc',
            'test/auto_test/standard/dtmf_test.cc',
            'test/auto_test/standard/encryption_test.cc',
            'test/auto_test/standard/external_media_test.cc',
            'test/auto_test/standard/file_before_streaming_test.cc',
            'test/auto_test/standard/file_test.cc',
            'test/auto_test/standard/hardware_before_initializing_test.cc',
            'test/auto_test/standard/hardware_before_streaming_test.cc',
            'test/auto_test/standard/hardware_test.cc',
            'test/auto_test/standard/manual_hold_test.cc',
            'test/auto_test/standard/mixing_test.cc',
            'test/auto_test/standard/neteq_stats_test.cc',
            'test/auto_test/standard/neteq_test.cc',
            'test/auto_test/standard/network_test.cc',
            'test/auto_test/standard/rtp_rtcp_before_streaming_test.cc',
            'test/auto_test/standard/rtp_rtcp_test.cc',
            'test/auto_test/standard/voe_base_misc_test.cc',
            'test/auto_test/standard/video_sync_test.cc',
            'test/auto_test/standard/volume_test.cc',
            'test/auto_test/resource_manager.cc',
            'test/auto_test/voe_cpu_test.cc',
            'test/auto_test/voe_cpu_test.h',
            'test/auto_test/voe_extended_test.cc',
            'test/auto_test/voe_extended_test.h',
            'test/auto_test/voe_standard_test.cc',
            'test/auto_test/voe_standard_test.h',
            'test/auto_test/voe_stress_test.cc',
            'test/auto_test/voe_stress_test.h',
            'test/auto_test/voe_test_defines.h',
            'test/auto_test/voe_test_interface.h',
            'test/auto_test/voe_unit_test.cc',
            'test/auto_test/voe_unit_test.h',
          ],
          'conditions': [
            ['OS=="android"', {
              # some tests are not supported on android yet, exclude these tests.
              'sources!': [
                'test/auto_test/standard/hardware_before_streaming_test.cc',
              ],
            }],
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
        },
        {
          # command line test that should work on linux/mac/win
          'target_name': 'voe_cmd_test',
          'type': 'executable',
          'dependencies': [
            'voice_engine',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:channel_transport',
            '<(webrtc_root)/test/test.gyp:test_support',
          ],
          'sources': [
            'test/cmd_test/voe_cmd_test.cc',
          ],
        },
      ], # targets
      'conditions': [
        # TODO(kjellander): Support UseoFMFC on VS2010.
        # http://code.google.com/p/webrtc/issues/detail?id=709
        ['OS=="win" and MSVS_VERSION < "2010"', {
          'targets': [
            # WinTest - GUI test for Windows
            {
              'target_name': 'voe_ui_win_test',
              'type': 'executable',
              'dependencies': [
                'voice_engine',
                '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
                '<(webrtc_root)/test/test.gyp:test_support',
              ],
              'sources': [
                'test/win_test/Resource.h',
                'test/win_test/WinTest.cc',
                'test/win_test/WinTest.h',
                'test/win_test/WinTest.rc',
                'test/win_test/WinTestDlg.cc',
                'test/win_test/WinTestDlg.h',
                'test/win_test/res/WinTest.ico',
                'test/win_test/res/WinTest.rc2',
                'test/win_test/stdafx.cc',
                'test/win_test/stdafx.h',
              ],
              'configurations': {
                'Common_Base': {
                  'msvs_configuration_attributes': {
                    'conditions': [
                      ['component=="shared_library"', {
                        'UseOfMFC': '2',  # Shared DLL
                      },{
                        'UseOfMFC': '1',  # Static
                      }],
                    ],
                  },
                },
              },
              'msvs_settings': {
                'VCLinkerTool': {
                  'SubSystem': '2',   # Windows
                },
              },
            },
          ],  # targets
        }],
        # TODO(henrike): remove build_with_chromium==1 when the bots are using
        # Chromium's buildbots.
        ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
          'targets': [
            {
              'target_name': 'voice_engine_unittests_apk_target',
              'type': 'none',
              'dependencies': [
                '<(apk_tests_path):voice_engine_unittests_apk',
              ],
            },
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'voice_engine_unittests_run',
              'type': 'none',
              'dependencies': [
                '<(import_isolate_path):import_isolate_gypi',
                'voice_engine_unittests',
              ],
              'includes': [
                'voice_engine_unittests.isolate',
              ],
              'sources': [
                'voice_engine_unittests.isolate',
              ],
            },
            {
              'target_name': 'voe_auto_test_run',
              'type': 'none',
              'dependencies': [
                '<(import_isolate_path):import_isolate_gypi',
                'voe_auto_test',
              ],
              'includes': [
                'voe_auto_test.isolate',
              ],
              'sources': [
                'voe_auto_test.isolate',
              ],
            },
          ],
        }],
      ],  # conditions
    }], # include_tests
  ], # conditions
}
