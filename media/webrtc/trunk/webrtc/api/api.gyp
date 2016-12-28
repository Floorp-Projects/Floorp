# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'conditions': [
    ['OS=="ios"', {
      'targets': [
        {
          'target_name': 'rtc_api_objc',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/base/base.gyp:rtc_base_objc',
            '../../talk/libjingle.gyp:libjingle_peerconnection',
          ],
          'sources': [
            'objc/RTCIceCandidate+Private.h',
            'objc/RTCIceCandidate.h',
            'objc/RTCIceCandidate.mm',
            'objc/RTCIceServer+Private.h',
            'objc/RTCIceServer.h',
            'objc/RTCIceServer.mm',
            'objc/RTCMediaConstraints+Private.h',
            'objc/RTCMediaConstraints.h',
            'objc/RTCMediaConstraints.mm',
            'objc/RTCMediaSource+Private.h',
            'objc/RTCMediaSource.h',
            'objc/RTCMediaSource.mm',
            'objc/RTCMediaStreamTrack+Private.h',
            'objc/RTCMediaStreamTrack.h',
            'objc/RTCMediaStreamTrack.mm',
            'objc/RTCOpenGLVideoRenderer.h',
            'objc/RTCOpenGLVideoRenderer.mm',
            'objc/RTCSessionDescription+Private.h',
            'objc/RTCSessionDescription.h',
            'objc/RTCSessionDescription.mm',
            'objc/RTCStatsReport+Private.h',
            'objc/RTCStatsReport.h',
            'objc/RTCStatsReport.mm',
            'objc/RTCVideoFrame+Private.h',
            'objc/RTCVideoFrame.h',
            'objc/RTCVideoFrame.mm',
            'objc/RTCVideoRenderer.h',
          ],
          'conditions': [
            ['OS=="ios"', {
              'sources': [
                'objc/RTCEAGLVideoView.h',
                'objc/RTCEAGLVideoView.m',
              ],
              'all_dependent_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework CoreGraphics',
                    '-framework GLKit',
                    '-framework OpenGLES',
                    '-framework QuartzCore',
                  ]
                }
              }
            }],
            ['OS=="mac"', {
              'sources': [
                'objc/RTCNSGLVideoView.h',
                'objc/RTCNSGLVideoView.m',
              ],
            }],
          ],
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
            'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'YES',
            'GCC_PREFIX_HEADER': 'objc/WebRTC-Prefix.pch',
          },
        }
      ],
    }], # OS=="ios"
  ],
}
