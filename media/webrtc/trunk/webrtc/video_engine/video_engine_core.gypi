# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'video_engine_core',
      'type': 'static_library',
      'dependencies': [

        # common_video
       '<(webrtc_root)/common_video/common_video.gyp:common_video',

        # ModulesShared
        '<(webrtc_root)/modules/modules.gyp:media_file',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
        '<(webrtc_root)/modules/modules.gyp:webrtc_utility',

        # ModulesVideo
        '<(webrtc_root)/modules/modules.gyp:bitrate_controller',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module',
        '<(webrtc_root)/modules/modules.gyp:webrtc_video_coding',
        '<(webrtc_root)/modules/modules.gyp:video_processing',
        '<(webrtc_root)/modules/modules.gyp:video_render_module',

        # VoiceEngine
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',

        # system_wrappers
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        # interface
        'include/vie_base.h',
        'include/vie_capture.h',
        'include/vie_codec.h',
        'include/vie_encryption.h',
        'include/vie_errors.h',
        'include/vie_external_codec.h',
        'include/vie_image_process.h',
        'include/vie_network.h',
        'include/vie_render.h',
        'include/vie_rtp_rtcp.h',

        # headers
        'call_stats.h',
        'encoder_state_feedback.h',
        'overuse_frame_detector.h',
        'stream_synchronization.h',
        'vie_base_impl.h',
        'vie_capture_impl.h',
        'vie_codec_impl.h',
        'vie_defines.h',
        'vie_encryption_impl.h',
        'vie_external_codec_impl.h',
        'vie_image_process_impl.h',
        'vie_impl.h',
        'vie_network_impl.h',
        'vie_ref_count.h',
        'vie_remb.h',
        'vie_render_impl.h',
        'vie_rtp_rtcp_impl.h',
        'vie_shared_data.h',
        'vie_capturer.h',
        'vie_channel.h',
        'vie_channel_group.h',
        'vie_channel_manager.h',
        'vie_encoder.h',
        'vie_file_image.h',
        'vie_frame_provider_base.h',
        'vie_input_manager.h',
        'vie_manager_base.h',
        'vie_receiver.h',
        'vie_renderer.h',
        'vie_render_manager.h',
        'vie_sender.h',
        'vie_sync_module.h',

        # ViE
        'call_stats.cc',
        'encoder_state_feedback.cc',
        'overuse_frame_detector.cc',
        'stream_synchronization.cc',
        'vie_base_impl.cc',
        'vie_capture_impl.cc',
        'vie_codec_impl.cc',
        'vie_encryption_impl.cc',
        'vie_external_codec_impl.cc',
        'vie_image_process_impl.cc',
        'vie_impl.cc',
        'vie_network_impl.cc',
        'vie_ref_count.cc',
        'vie_render_impl.cc',
        'vie_rtp_rtcp_impl.cc',
        'vie_shared_data.cc',
        'vie_capturer.cc',
        'vie_channel.cc',
        'vie_channel_group.cc',
        'vie_channel_manager.cc',
        'vie_encoder.cc',
        'vie_file_image.cc',
        'vie_frame_provider_base.cc',
        'vie_input_manager.cc',
        'vie_manager_base.cc',
        'vie_receiver.cc',
        'vie_remb.cc',
        'vie_renderer.cc',
        'vie_render_manager.cc',
        'vie_sender.cc',
        'vie_sync_module.cc',

        # New VideoEngine API
        'internal/call.cc',
        'internal/call.h',
        'internal/transport_adapter.cc',
        'internal/transport_adapter.h',
        'internal/video_receive_stream.cc',
        'internal/video_receive_stream.h',
        'internal/video_send_stream.cc',
        'internal/video_send_stream.h',
        'new_include/call.h',
        'new_include/config.h',
        'new_include/frame_callback.h',
        'new_include/transport.h',
        'new_include/video_receive_stream.h',
        'new_include/video_renderer.h',
        'new_include/video_send_stream.h',
      ], # source
      # TODO(jschuh): Bug 1348: fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'video_engine_core_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'video_engine_core',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'call_stats_unittest.cc',
            'encoder_state_feedback_unittest.cc',
            'overuse_frame_detector_unittest.cc',
            'stream_synchronization_unittest.cc',
            'vie_remb_unittest.cc',
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
      ], # targets
      'conditions': [
        # TODO(henrike): remove build_with_chromium==1 when the bots are using
        # Chromium's buildbots.
        ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
          'targets': [
            {
              'target_name': 'video_engine_core_unittests_apk_target',
              'type': 'none',
              'dependencies': [
                '<(apk_tests_path):video_engine_core_unittests_apk',
              ],
            },
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'video_engine_core_unittests_run',
              'type': 'none',
              'dependencies': [
                '<(import_isolate_path):import_isolate_gypi',
                'video_engine_core_unittests',
              ],
              'includes': [
                'video_engine_core_unittests.isolate',
              ],
              'sources': [
                'video_engine_core_unittests.isolate',
              ],
            },
          ],
        }],
      ],
    }], # include_tests
  ], # conditions
}
