# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
      'type': '<(library)',
      'dependencies': [

        # common_video
       '<(webrtc_root)/common_video/common_video.gyp:webrtc_libyuv',
       '<(webrtc_root)/common_video/common_video.gyp:webrtc_jpeg',

        # ModulesShared
        '<(webrtc_root)/modules/modules.gyp:media_file',
        '<(webrtc_root)/modules/modules.gyp:rtp_rtcp',
        '<(webrtc_root)/modules/modules.gyp:udp_transport',
        '<(webrtc_root)/modules/modules.gyp:webrtc_utility',

        # ModulesVideo
        '<(webrtc_root)/modules/modules.gyp:video_capture_module',
        '<(webrtc_root)/modules/modules.gyp:webrtc_video_coding',
        '<(webrtc_root)/modules/modules.gyp:video_processing',
        '<(webrtc_root)/modules/modules.gyp:video_render_module',

        # VoiceEngine
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine_core',

        # system_wrappers
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        'include',
        '../common_video/interface',
        '../modules/video_capture/main/interface',
        '../modules/video_render/main/interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        # interface
        'include/vie_base.h',
        'include/vie_capture.h',
        'include/vie_codec.h',
        'include/vie_encryption.h',
        'include/vie_errors.h',
        'include/vie_external_codec.h',
        'include/vie_file.h',
        'include/vie_image_process.h',
        'include/vie_network.h',
        'include/vie_render.h',
        'include/vie_rtp_rtcp.h',

        # headers
        'vie_base_impl.h',
        'vie_capture_impl.h',
        'vie_codec_impl.h',
        'vie_defines.h',
        'vie_encryption_impl.h',
        'vie_external_codec_impl.h',
        'vie_file_impl.h',
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
        'vie_file_player.h',
        'vie_file_recorder.h',
        'vie_frame_provider_base.h',
        'vie_input_manager.h',
        'vie_manager_base.h',
        'vie_performance_monitor.h',
        'vie_receiver.h',
        'vie_renderer.h',
        'vie_render_manager.h',
        'vie_sender.h',
        'vie_sync_module.h',

        # ViE
        'vie_base_impl.cc',
        'vie_capture_impl.cc',
        'vie_codec_impl.cc',
        'vie_encryption_impl.cc',
        'vie_external_codec_impl.cc',
        'vie_file_impl.cc',
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
        'vie_file_player.cc',
        'vie_file_recorder.cc',
        'vie_frame_provider_base.cc',
        'vie_input_manager.cc',
        'vie_manager_base.cc',
        'vie_performance_monitor.cc',
        'vie_receiver.cc',
        'vie_remb.cc',
        'vie_renderer.cc',
        'vie_render_manager.cc',
        'vie_sender.cc',
        'vie_sync_module.cc',
      ], # source
    },
  ], # targets
  'conditions': [
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'video_engine_core_unittests',
          'type': 'executable',
          'dependencies': [
            'video_engine_core',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/../testing/gmock.gyp:gmock',
            '<(webrtc_root)/../test/test.gyp:test_support_main',
          ],
          'include_dirs': [
            '..',
            '../modules/interface',
            '../modules/rtp_rtcp/interface',
          ],
          'sources': [
            'vie_remb_unittest.cc',
          ],
        },
      ], # targets
    }], # build_with_chromium
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
