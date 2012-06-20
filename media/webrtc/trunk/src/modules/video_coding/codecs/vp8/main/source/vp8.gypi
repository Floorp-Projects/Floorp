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
      'target_name': 'webrtc_vp8',
      'type': '<(library)',
      'dependencies': [
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/common_video/common_video.gyp:webrtc_libyuv',
      ],
      'include_dirs': [
        '../interface',
        '<(webrtc_root)/common_video/interface',
        '<(webrtc_root)/modules/video_coding/codecs/interface',
        '<(webrtc_root)/modules/interface',
      ],
      'conditions': [
        ['build_with_chromium==1', {
          'dependencies': [
            '<(webrtc_root)/../libvpx/libvpx.gyp:libvpx',
          ],
          'defines': [
            'WEBRTC_LIBVPX_VERSION=960' # Bali
          ],
        },{
          'conditions': [
             ['build_with_mozilla==1', {
               'dependencies': [
#                 '<(webrtc_root)/../third_party/libvpx/libvpx.gyp:libvpx_include',
               ],
               'include_dirs': [
                 '$(DIST)/include',
               ],
               'defines': [
                 # This must be updated to match mozilla's version of libvpx
                 'WEBRTC_LIBVPX_VERSION=971',
                 'WEBRTC_LIBVPX_TEMPORAL_LAYERS=0'
               ],
               'link_settings': {
                 'libraries': [
                    '$(LIBVPX_OBJ)/libvpx.a',
                 ],
               },
             },{
               'dependencies': [
                 '<(webrtc_root)/../third_party/libvpx/libvpx.gyp:libvpx',
               ],
               'defines': [
                 'WEBRTC_LIBVPX_VERSION=971', # Cayuga
                 'WEBRTC_LIBVPX_TEMPORAL_LAYERS=1'
               ],
               'sources': [
                 'temporal_layers.h',
                 'temporal_layers.cc',
               ],
             }],
           ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '<(webrtc_root)/common_video/interface',
          '<(webrtc_root)/modules/video_coding/codecs/interface',
        ],
      },
      'sources': [
        'reference_picture_selection.h',
        'reference_picture_selection.cc',
        '../interface/vp8.h',
        '../interface/vp8_common_types.h',
        'vp8.cc',
      ],
    },
  ], # targets
  # Exclude the test target when building with chromium.
  'conditions': [   
    ['build_with_chromium==0 and build_with_mozilla==0', {
      'targets': [
        {
          'target_name': 'vp8_test',
          'type': 'executable',
          'dependencies': [
            'test_framework',
            'webrtc_vp8',
            '<(webrtc_root)/common_video/common_video.gyp:webrtc_libyuv',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/../test/test.gyp:test_support',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
          ],
         'sources': [
            # header files
            '../test/benchmark.h',
            '../test/dual_decoder_test.h',
            '../test/normal_async_test.h',
            '../test/packet_loss_test.h',
            '../test/rps_test.h',
            '../test/unit_test.h',

           # source files
            '../test/benchmark.cc',
            '../test/dual_decoder_test.cc',
            '../test/normal_async_test.cc',
            '../test/packet_loss_test.cc',
            '../test/rps_test.cc',
            '../test/tester.cc',
            '../test/unit_test.cc',
          ],
        },
        {
          'target_name': 'vp8_unittests',
          'type': 'executable',
          'dependencies': [
            '<(webrtc_root)/../test/test.gyp:test_support_main',
            '<(webrtc_root)/../testing/gtest.gyp:gtest',
            '<(webrtc_root)/../third_party/libvpx/libvpx.gyp:libvpx',
            'webrtc_vp8',
          ],
          'include_dirs': [
            '<(webrtc_root)/../third_party/libvpx/source/libvpx',
          ],
          'sources': [
            'reference_picture_selection_unittest.cc',
            'temporal_layers_unittest.cc',
          ],
        },
      ], # targets
    }], # build_with_chromium
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
