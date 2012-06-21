# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ 'src/build/common.gypi', ],
  'variables': {
    'peerconnection_sample': 'third_party/libjingle/source/talk/examples/peerconnection',
  },  

  'targets': [
    {
      'target_name': 'peerconnection_server',
      'type': 'executable',
      'sources': [
        '<(peerconnection_sample)/server/data_socket.cc',
        '<(peerconnection_sample)/server/data_socket.h',
        '<(peerconnection_sample)/server/main.cc',
        '<(peerconnection_sample)/server/peer_channel.cc',
        '<(peerconnection_sample)/server/peer_channel.h',
        '<(peerconnection_sample)/server/utils.cc',
        '<(peerconnection_sample)/server/utils.h',
      ],
      'include_dirs': [
        'third_party/libjingle/source',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'peerconnection_client',
          'type': 'executable',
          'sources': [
            '<(peerconnection_sample)/client/conductor.cc',
            '<(peerconnection_sample)/client/conductor.h',
            '<(peerconnection_sample)/client/defaults.cc',
            '<(peerconnection_sample)/client/defaults.h',
            '<(peerconnection_sample)/client/main.cc',
            '<(peerconnection_sample)/client/main_wnd.cc',
            '<(peerconnection_sample)/client/main_wnd.h',
            '<(peerconnection_sample)/client/peer_connection_client.cc',
            '<(peerconnection_sample)/client/peer_connection_client.h',
            'third_party/libjingle/source/talk/base/win32socketinit.cc',
            'third_party/libjingle/source/talk/base/win32socketserver.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
             'SubSystem': '2',  # Windows
            },
          },
          'dependencies': [
            'third_party/libjingle/libjingle.gyp:libjingle_app',
          ],
          'include_dirs': [
            'src',
            'src/modules/interface',
            'third_party/libjingle/source',
          ],
        },
      ],  # targets
    }, ],  # OS="win"
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'peerconnection_client',
          'type': 'executable',
          'sources': [
            '<(peerconnection_sample)/client/conductor.cc',
            '<(peerconnection_sample)/client/conductor.h',
            '<(peerconnection_sample)/client/defaults.cc',
            '<(peerconnection_sample)/client/defaults.h',
            '<(peerconnection_sample)/client/linux/main.cc',
            '<(peerconnection_sample)/client/linux/main_wnd.cc',
            '<(peerconnection_sample)/client/linux/main_wnd.h',
            '<(peerconnection_sample)/client/peer_connection_client.cc',
            '<(peerconnection_sample)/client/peer_connection_client.h',
          ],
          'dependencies': [
            'third_party/libjingle/libjingle.gyp:libjingle_app',
            # TODO(tommi): Switch to this and remove specific gtk dependency
            # sections below for cflags and link_settings.
            # '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'include_dirs': [
            'src',
            'src/modules/interface',
            'third_party/libjingle/source',
          ],
          'cflags': [
            '<!@(pkg-config --cflags gtk+-2.0)',
          ],
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other gtk+-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l gtk+-2.0 gthread-2.0)',
              '-lX11',
              '-lXext',
            ],
          },
        },
      ],  # targets
    }, ],  # OS="linux"
  ],

}
