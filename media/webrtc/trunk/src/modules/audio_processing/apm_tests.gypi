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
      'target_name': 'audioproc_unittest',
      'type': 'executable',
      'conditions': [
        ['prefer_fixed_point==1', {
          'defines': [ 'WEBRTC_AUDIOPROC_FIXED_PROFILE' ],
        }, {
          'defines': [ 'WEBRTC_AUDIOPROC_FLOAT_PROFILE' ],
        }],
        ['enable_protobuf==1', {
          'defines': [ 'WEBRTC_AUDIOPROC_DEBUG_DUMP' ],
        }],
      ],
      'dependencies': [
        'audio_processing',
        'audioproc_unittest_proto',
        '<(webrtc_root)/common_audio/common_audio.gyp:signal_processing',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/../test/test.gyp:test_support',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'aec/system_delay_unittest.cc',
        'test/unit_test.cc',
      ],
    },
    {
      'target_name': 'audioproc_unittest_proto',
      'type': 'static_library',
      'sources': [ 'test/unittest.proto', ],
      'variables': {
        'proto_in_dir': 'test',
        # Workaround to protect against gyp's pathname relativization when this
        # file is included by modules.gyp.
        'proto_out_protected': 'webrtc/audio_processing',
        'proto_out_dir': '<(proto_out_protected)',
      },
      'includes': [ '../../build/protoc.gypi', ],
    },
  ],
  'conditions': [
    ['enable_protobuf==1', {
      'targets': [
        {
          'target_name': 'audioproc',
          'type': 'executable',
          'dependencies': [
            'audio_processing',
            'audioproc_debug_proto',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
          ],
          'sources': [ 'test/process_test.cc', ],
        },
        {
          'target_name': 'unpack_aecdump',
          'type': 'executable',
          'dependencies': [
            'audioproc_debug_proto',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/../third_party/google-gflags/google-gflags.gyp:google-gflags',
          ],
          'sources': [ 'test/unpack.cc', ],
        },
      ],
    }],
  ],
}
