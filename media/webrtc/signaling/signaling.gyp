# Copyright (c) 2011, The WebRTC project authors. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  *  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#
#  *  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#
#  *  Neither the name of Google nor the names of its contributors may
#     be used to endorse or promote products derived from this software
#     without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Indent 2 spaces, no tabs.
#
#
# sip.gyp - a library for SIP
#

{
  'variables': {
    'chromium_code': 1,
  },

  'target_defaults': {
    'conditions': [
      ['moz_widget_toolkit_gonk==1', {
        'defines' : [
          'WEBRTC_GONK',
       ],
      }],
    ],
  },

  'targets': [

    #
    # ECC
    #
    {
      'target_name': 'ecc',
      'type': 'static_library',

      #
      # INCLUDES
      #
      'include_dirs': [
        '..',
        './src',
        './src/common',
        './src/common/browser_logging',
        './src/common/time_profiling',
        './src/media',
        './src/media-conduit',
        './src/mediapipeline',
        './src/peerconnection',
        './src/sdp/sipcc',
        '../../../dom/base',
        '../../../dom/media',
        '../../../media/mtransport',
        '../trunk',
        '../../libyuv/libyuv/include',
        '../../mtransport/third_party/nrappkit/src/util/libekr',
      ],

      #
      # DEPENDENCIES
      #
      'dependencies': [
      ],

      'export_dependent_settings': [
      ],


      #
      # SOURCES
      #
      'sources': [
        # Media Conduit
        './src/media-conduit/AudioConduit.h',
        './src/media-conduit/AudioConduit.cpp',
        './src/media-conduit/VideoConduit.h',
        './src/media-conduit/VideoConduit.cpp',
        './src/media-conduit/RunningStat.h',
        # Common
        './src/common/CommonTypes.h',
        './src/common/csf_common.h',
        './src/common/NullDeleter.h',
        './src/common/PtrVector.h',
        './src/common/Wrapper.h',
        './src/common/NullTransport.h',
        './src/common/YuvStamper.cpp',
        # Browser Logging
        './src/common/browser_logging/CSFLog.cpp',
        './src/common/browser_logging/CSFLog.h',
        './src/common/browser_logging/WebRtcLog.cpp',
        './src/common/browser_logging/WebRtcLog.h',
        # Browser Logging
        './src/common/time_profiling/timecard.c',
        './src/common/time_profiling/timecard.h',
        # PeerConnection
        './src/peerconnection/MediaPipelineFactory.cpp',
        './src/peerconnection/MediaPipelineFactory.h',
        './src/peerconnection/PeerConnectionCtx.cpp',
        './src/peerconnection/PeerConnectionCtx.h',
        './src/peerconnection/PeerConnectionImpl.cpp',
        './src/peerconnection/PeerConnectionImpl.h',
        './src/peerconnection/PeerConnectionMedia.cpp',
        './src/peerconnection/PeerConnectionMedia.h',
        # Media pipeline
        './src/mediapipeline/MediaPipeline.h',
        './src/mediapipeline/MediaPipeline.cpp',
        './src/mediapipeline/MediaPipelineFilter.h',
        './src/mediapipeline/MediaPipelineFilter.cpp',
        './src/mediapipeline/RtpLogger.h',
        './src/mediapipeline/RtpLogger.cpp',
         # SDP
         './src/sdp/sipcc/ccsdp.h',
         './src/sdp/sipcc/cpr_string.c',
         './src/sdp/sipcc/sdp_access.c',
         './src/sdp/sipcc/sdp_attr.c',
         './src/sdp/sipcc/sdp_attr_access.c',
         './src/sdp/sipcc/sdp_base64.c',
         './src/sdp/sipcc/sdp_config.c',
         './src/sdp/sipcc/sdp_main.c',
         './src/sdp/sipcc/sdp_token.c',
         './src/sdp/sipcc/sdp.h',
         './src/sdp/sipcc/sdp_base64.h',
         './src/sdp/sipcc/sdp_os_defs.h',
         './src/sdp/sipcc/sdp_private.h',
         './src/sdp/sipcc/sdp_utils.c',
         './src/sdp/sipcc/sdp_services_unix.c',

         # SDP Wrapper
         './src/sdp/Sdp.h',
         './src/sdp/SdpAttribute.h',
         './src/sdp/SdpAttribute.cpp',
         './src/sdp/SdpAttributeList.h',
         './src/sdp/SdpErrorHolder.h',
         './src/sdp/SdpHelper.h',
         './src/sdp/SdpHelper.cpp',
         './src/sdp/SdpMediaSection.h',
         './src/sdp/SdpMediaSection.cpp',
         './src/sdp/SipccSdp.h',
         './src/sdp/SipccSdpAttributeList.h',
         './src/sdp/SipccSdpAttributeList.cpp',
         './src/sdp/SipccSdpMediaSection.h',
         './src/sdp/SipccSdpParser.h',
         './src/sdp/SipccSdp.cpp',
         './src/sdp/SipccSdpMediaSection.cpp',
         './src/sdp/SipccSdpParser.cpp',

         # JSEP
         './src/jsep/JsepCodecDescription.h',
         './src/jsep/JsepSession.h',
         './src/jsep/JsepSessionImpl.cpp',
         './src/jsep/JsepSessionImpl.h',
         './src/jsep/JsepTrack.cpp',
         './src/jsep/JsepTrack.h',
         './src/jsep/JsepTrackEncoding.h',
         './src/jsep/JsepTransport.h'
      ],

      #
      # DEFINES
      #

      'defines' : [
        'LOG4CXX_STATIC',
        '_NO_LOG4CXX',
        'USE_SSLEAY',
        '_CPR_USE_EXTERNAL_LOGGER',
        'WEBRTC_RELATIVE_PATH',
        'HAVE_WEBRTC_VIDEO',
        'HAVE_WEBRTC_VOICE',
        'HAVE_STDINT_H=1',
        'HAVE_STDLIB_H=1',
        'HAVE_UINT8_T=1',
        'HAVE_UINT16_T=1',
        'HAVE_UINT32_T=1',
        'HAVE_UINT64_T=1',
      ],

      'cflags_mozilla': [
        '$(NSPR_CFLAGS)',
        '$(NSS_CFLAGS)',
        '$(MOZ_PIXMAN_CFLAGS)',
      ],


      #
      # Conditionals
      #
      'conditions': [
        # hack so I can change the include flow for SrtpFlow
        ['build_with_mozilla==1', {
          'sources': [
            './src/mediapipeline/SrtpFlow.h',
            './src/mediapipeline/SrtpFlow.cpp',
          ],
          'include_dirs!': [
            '../trunk/webrtc',
          ],
          'include_dirs': [
            '../../../netwerk/srtp/src/include',
            '../../../netwerk/srtp/src/crypto/include',
          ],
        }],
        ['moz_webrtc_omx==1', {
          'sources': [
            './src/media-conduit/WebrtcOMXH264VideoCodec.cpp',
            './src/media-conduit/OMXVideoCodec.cpp',
          ],
          'include_dirs': [
            # hack on hack to re-add it after SrtpFlow removes it
            '../../../dom/media/omx',
            '../../../gfx/layers/client',
          ],
          'cflags_mozilla': [
            '-I$(ANDROID_SOURCE)/frameworks/av/include/media/stagefright',
            '-I$(ANDROID_SOURCE)/frameworks/av/include',
            '-I$(ANDROID_SOURCE)/frameworks/native/include/media/openmax',
            '-I$(ANDROID_SOURCE)/frameworks/native/include',
            '-I$(ANDROID_SOURCE)/frameworks/native/opengl/include',
          ],
          'defines' : [
            'MOZ_WEBRTC_OMX'
          ],
        }],
        ['moz_webrtc_mediacodec==1', {
          'include_dirs': [
            '../../../widget/android',
          ],
          'sources': [
            './src/media-conduit/MediaCodecVideoCodec.h',
            './src/media-conduit/WebrtcMediaCodecVP8VideoCodec.h',
            './src/media-conduit/MediaCodecVideoCodec.cpp',
            './src/media-conduit/WebrtcMediaCodecVP8VideoCodec.cpp',
          ],
          'defines' : [
            'MOZ_WEBRTC_MEDIACODEC',
          ],
        }],
        ['(build_for_test==0) and (build_for_standalone==0)', {
          'sources': [
            './src/peerconnection/MediaStreamList.cpp',
            './src/peerconnection/MediaStreamList.h',
            './src/peerconnection/WebrtcGlobalInformation.cpp',
            './src/peerconnection/WebrtcGlobalInformation.h',
          ],
        }],
        ['build_for_test!=0', {
          'include_dirs': [
            './test'
          ],
          'defines' : [
            'NO_CHROMIUM_LOGGING',
            'USE_FAKE_PCOBSERVER',
          ],
        }],
        ['build_for_standalone==0', {
          'sources': [
            './src/media-conduit/GmpVideoCodec.cpp',
            './src/media-conduit/WebrtcGmpVideoCodec.cpp',
          ],
        }],
        ['build_for_standalone!=0', {
          'include_dirs': [
            './test'
          ],
          'defines' : [
            'NO_CHROMIUM_LOGGING',
            'USE_FAKE_PCOBSERVER',
          ],
        }],
        ['(OS=="linux") or (OS=="android")', {
          'include_dirs': [
          ],

          'defines': [
            'OS_LINUX',
            'SIP_OS_LINUX',
            'WEBRTC_POSIX',
            '_GNU_SOURCE',
            'LINUX',
            'GIPS_VER=3510',
            'SECLIB_OPENSSL',
            'WEBRTC_BUILD_LIBEVENT',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['OS=="android" or moz_widget_toolkit_gonk==1', {
          'cflags_mozilla': [
            # This warning complains about important MOZ_EXPORT attributes
            # on forward declarations for Android API types.
            '-Wno-error=attributes',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
          ],
          'defines': [
            'OS_WIN',
            'SIP_OS_WINDOWS',
            'WEBRTC_WIN',
            'WIN32',
            'GIPS_VER=3480',
            'SIPCC_BUILD',
            'HAVE_WINSOCK2_H'
          ],

          'cflags_mozilla': [
          ],
        }],
        ['os_bsd==1', {
          'include_dirs': [
          ],
          'defines': [
            # avoiding pointless ifdef churn
            'WEBRTC_POSIX',
            'SIP_OS_OSX',
            'OSX',
            'SECLIB_OPENSSL',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['OS=="mac" or OS=="ios"', {
          'include_dirs': [
          ],
          'defines': [
            'WEBRTC_POSIX',
            'WEBRTC_MAC',
            'OS_MACOSX',
            'SIP_OS_OSX',
            'OSX',
            '_FORTIFY_SOURCE=2',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['clang == 1', {
          'cflags_mozilla': [
            '-Wno-inconsistent-missing-override',
            '-Wno-macro-redefined',
         ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
