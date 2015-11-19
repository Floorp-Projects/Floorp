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
      'target_name': 'webrtc_utility',
      'type': 'static_library',
      'dependencies': [
        'audio_coding_module',
        'media_file',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'interface/audio_frame_operations.h',
        'interface/file_player.h',
        'interface/file_recorder.h',
        'interface/helpers_android.h',
        'interface/process_thread.h',
        'interface/rtp_dump.h',
        'source/audio_frame_operations.cc',
        'source/coder.cc',
        'source/coder.h',
        'source/file_player_impl.cc',
        'source/file_player_impl.h',
        'source/file_recorder_impl.cc',
        'source/file_recorder_impl.h',
        'source/helpers_android.cc',
        'source/process_thread_impl.cc',
        'source/process_thread_impl.h',
        'source/rtp_dump_impl.cc',
        'source/rtp_dump_impl.h',
      ],
    },
  ], # targets
}
