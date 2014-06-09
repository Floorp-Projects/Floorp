# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
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
        './src/callcontrol',
        './src/common',
        './src/common/browser_logging',
        './src/common/time_profiling',
        './src/media',
        './src/media-conduit',
        './src/mediapipeline',
        './src/softphonewrapper',
        './src/peerconnection',
        './include',
        './src/sipcc/include',
        './src/sipcc/cpr/include',
        '../../../ipc/chromium/src/base/third_party/nspr',
        '../../../xpcom/base',
        '../../../dom/base',
        '../../../content/media',
        '../../../media/mtransport',
        '../trunk',
        '../trunk/webrtc',
        '../trunk/webrtc/video_engine/include',
        '../trunk/webrtc/voice_engine/include',
        '../trunk/webrtc/modules/interface',
        '../trunk/webrtc/peerconnection',
        '../../libyuv/include',
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
        './src/media-conduit/CodecStatistics.h',
        './src/media-conduit/CodecStatistics.cpp',
        './src/media-conduit/RunningStat.h',
        './src/media-conduit/GmpVideoCodec.cpp',
        './src/media-conduit/WebrtcGmpVideoCodec.cpp',
        # Common
        './src/common/CommonTypes.h',
        './src/common/csf_common.h',
        './src/common/NullDeleter.h',
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
        # Call Control
        './src/callcontrol/CC_CallTypes.cpp',
        './src/callcontrol/CallControlManager.cpp',
        './src/callcontrol/CallControlManagerImpl.cpp',
        './src/callcontrol/ECC_Types.cpp',
        './src/callcontrol/PhoneDetailsImpl.cpp',
        './src/callcontrol/debug-psipcc-types.cpp',
        './src/callcontrol/CallControlManagerImpl.h',
        './src/callcontrol/PhoneDetailsImpl.h',
        # Media
        './src/media/CSFAudioControlWrapper.cpp',
        './src/media/CSFVideoControlWrapper.cpp',
        './src/media/VcmSIPCCBinding.cpp',
        './src/media/cip_mmgr_mediadefinitions.h',
        './src/media/cip_Sipcc_CodecMask.h',
        './src/media/CSFAudioControlWrapper.h',
        './src/media/CSFAudioTermination.h',
        './src/media/CSFMediaProvider.h',
        './src/media/CSFMediaTermination.h',
        './src/media/CSFToneDefinitions.h',
        './src/media/CSFVideoCallMediaControl.h',
        './src/media/CSFVideoControlWrapper.h',
        './src/media/CSFVideoTermination.h',
        './src/media/VcmSIPCCBinding.h',
        # SoftPhoneWrapper
        './src/softphonewrapper/CC_SIPCCCall.cpp',
        './src/softphonewrapper/CC_SIPCCCallInfo.cpp',
        './src/softphonewrapper/CC_SIPCCCallServerInfo.cpp',
        './src/softphonewrapper/CC_SIPCCDevice.cpp',
        './src/softphonewrapper/CC_SIPCCDeviceInfo.cpp',
        './src/softphonewrapper/CC_SIPCCFeatureInfo.cpp',
        './src/softphonewrapper/CC_SIPCCLine.cpp',
        './src/softphonewrapper/CC_SIPCCLineInfo.cpp',
        './src/softphonewrapper/CC_SIPCCService.cpp',
        './src/softphonewrapper/ccapi_plat_api_impl.cpp',
        './src/softphonewrapper/CC_SIPCCCall.h',
        './src/softphonewrapper/CC_SIPCCCallInfo.h',
        './src/softphonewrapper/CC_SIPCCCallServerInfo.h',
        './src/softphonewrapper/CC_SIPCCDevice.h',
        './src/softphonewrapper/CC_SIPCCDeviceInfo.h',
        './src/softphonewrapper/CC_SIPCCFeatureInfo.h',
        './src/softphonewrapper/CC_SIPCCLine.h',
        './src/softphonewrapper/CC_SIPCCLineInfo.h',
        './src/softphonewrapper/CC_SIPCCService.h',
        # PeerConnection
        './src/peerconnection/MediaStreamList.cpp',
        './src/peerconnection/MediaStreamList.h',
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
            '../../webrtc/trunk/webrtc',
            '../../../content/media/omx',
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
        ['build_for_test==0', {
          'defines' : [
            'MOZILLA_INTERNAL_API'
          ],
          'sources': [
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
            'USE_FAKE_MEDIA_STREAMS',
            'USE_FAKE_PCOBSERVER'
          ],
        }],
        ['(OS=="linux") or (OS=="android")', {
          'include_dirs': [
          ],

          'defines': [
            'OS_LINUX',
            'SIP_OS_LINUX',
            '_GNU_SOURCE',
            'LINUX',
            'GIPS_VER=3510',
            'SECLIB_OPENSSL',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
          ],
          'defines': [
            'OS_WIN',
            'SIP_OS_WINDOWS',
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
            'SIP_OS_OSX',
            'OSX',
            'SECLIB_OPENSSL',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
          ],
          'defines': [
            'OS_MACOSX',
            'SIP_OS_OSX',
            'OSX',
            '_FORTIFY_SOURCE=2',
          ],

          'cflags_mozilla': [
          ],
        }],
      ],
    },

    #
    # SIPCC
    #
    {
      'target_name': 'sipcc',
      'type': 'static_library',

      #
      # INCLUDES
      #
      'include_dirs': [
        './src/common/browser_logging',
        './src/common/time_profiling',
        './src/sipcc/include',
        './src/sipcc/core/includes',
        './src/sipcc/cpr/include',
        './src/sipcc/core/common',
        './src/sipcc/core/sipstack/h',
        './src/sipcc/core/ccapp',
        './src/sipcc/core/sdp',
        './src/sipcc/core/gsm/h',
        './src/sipcc/plat/common',
        '../../../media/mtransport',
        '../../../dom/base',
        '../trunk/third_party/libsrtp/srtp/include',
        '../trunk/third_party/libsrtp/srtp/crypto/include',
        # Danger: this is to include config.h. This could be bad.
        '../trunk/third_party/libsrtp/config',
        '../../../netwerk/sctp/datachannel',
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
        # CCAPP
        './src/sipcc/core/ccapp/call_logger.c',
        './src/sipcc/core/ccapp/call_logger.h',
        './src/sipcc/core/ccapp/capability_set.c',
        './src/sipcc/core/ccapp/capability_set.h',
        './src/sipcc/core/ccapp/cc_blf.c',
        './src/sipcc/core/ccapp/cc_call_feature.c',
        './src/sipcc/core/ccapp/cc_config.c',
        './src/sipcc/core/ccapp/cc_device_feature.c',
        './src/sipcc/core/ccapp/cc_device_manager.c',
        './src/sipcc/core/ccapp/cc_device_manager.h',
        './src/sipcc/core/ccapp/cc_info.c',
        './src/sipcc/core/ccapp/cc_service.c',
        './src/sipcc/core/ccapp/ccapi_call.c',
        './src/sipcc/core/ccapp/ccapi_call_info.c',
        './src/sipcc/core/ccapp/ccapi_config.c',
        './src/sipcc/core/ccapp/ccapi_device.c',
        './src/sipcc/core/ccapp/ccapi_device_info.c',
        './src/sipcc/core/ccapp/ccapi_feature_info.c',
        './src/sipcc/core/ccapp/ccapi_line.c',
        './src/sipcc/core/ccapp/ccapi_line_info.c',
        './src/sipcc/core/ccapp/ccapi_service.c',
        './src/sipcc/core/ccapp/ccapi_snapshot.c',
        './src/sipcc/core/ccapp/ccapi_snapshot.h',
        './src/sipcc/core/ccapp/ccapp_task.c',
        './src/sipcc/core/ccapp/ccapp_task.h',
        './src/sipcc/core/ccapp/ccprovider.c',
        './src/sipcc/core/ccapp/CCProvider.h',
        './src/sipcc/core/ccapp/conf_roster.c',
        './src/sipcc/core/ccapp/conf_roster.h',
        './src/sipcc/core/ccapp/sessionHash.c',
        './src/sipcc/core/ccapp/sessionHash.h',
        # COMMON
        './src/sipcc/core/common/cfgfile_utils.c',
        './src/sipcc/core/common/cfgfile_utils.h',
        './src/sipcc/core/common/config_api.c',
        './src/sipcc/core/common/config_parser.c',
        './src/sipcc/core/common/config_parser.h',
        './src/sipcc/core/common/init.c',
        './src/sipcc/core/common/logger.c',
        './src/sipcc/core/common/logger.h',
        './src/sipcc/core/common/logmsg.h',
        './src/sipcc/core/common/misc.c',
        './src/sipcc/core/common/plat.c',
        './src/sipcc/core/common/platform_api.c',
        './src/sipcc/core/common/prot_cfgmgr_private.h',
        './src/sipcc/core/common/prot_configmgr.c',
        './src/sipcc/core/common/prot_configmgr.h',
        './src/sipcc/core/common/resource_manager.c',
        './src/sipcc/core/common/resource_manager.h',
        './src/sipcc/core/common/sip_socket_api.c',
        './src/sipcc/core/common/subscription_handler.c',
        './src/sipcc/core/common/subscription_handler.h',
        './src/sipcc/core/common/text_strings.c',
        './src/sipcc/core/common/text_strings.h',
        './src/sipcc/core/common/thread_monitor.h',
        './src/sipcc/core/common/thread_monitor.c',
        './src/sipcc/core/common/ui.c',
        # GSM
        './src/sipcc/core/gsm/ccapi.c',
        './src/sipcc/core/gsm/ccapi_strings.c',
        './src/sipcc/core/gsm/dcsm.c',
        './src/sipcc/core/gsm/fim.c',
        './src/sipcc/core/gsm/fsm.c',
        './src/sipcc/core/gsm/fsmb2bcnf.c',
        './src/sipcc/core/gsm/fsmcac.c',
        './src/sipcc/core/gsm/fsmcnf.c',
        './src/sipcc/core/gsm/fsmdef.c',
        './src/sipcc/core/gsm/fsmxfr.c',
        './src/sipcc/core/gsm/gsm.c',
        './src/sipcc/core/gsm/gsm_sdp.c',
        './src/sipcc/core/gsm/gsm_sdp_crypto.c',
        './src/sipcc/core/gsm/lsm.c',
        './src/sipcc/core/gsm/media_cap_tbl.c',
        './src/sipcc/core/gsm/sm.c',
        './src/sipcc/core/gsm/subapi.c',
        './src/sipcc/core/gsm/h/fim.h',
        './src/sipcc/core/gsm/h/fsm.h',
        './src/sipcc/core/gsm/h/gsm.h',
        './src/sipcc/core/gsm/h/gsm_sdp.h',
        './src/sipcc/core/gsm/h/lsm.h',
        './src/sipcc/core/gsm/h/lsm_private.h',
        './src/sipcc/core/gsm/h/sm.h',
        # CORE INCLUDES
        './src/sipcc/core/includes/ccSesion.h',
        './src/sipcc/core/includes/ccapi.h',
        './src/sipcc/core/includes/check_sync.h',
        './src/sipcc/core/includes/ci.h',
        './src/sipcc/core/includes/codec_mask.h',
        './src/sipcc/core/includes/config.h',
        './src/sipcc/core/includes/configapp.h',
        './src/sipcc/core/includes/configmgr.h',
        './src/sipcc/core/includes/debug.h',
        './src/sipcc/core/includes/dialplan.h',
        './src/sipcc/core/includes/dialplanint.h',
        './src/sipcc/core/includes/digcalc.h',
        './src/sipcc/core/includes/dns_utils.h',
        './src/sipcc/core/includes/dtmf.h',
        './src/sipcc/core/includes/embedded.h',
        './src/sipcc/core/includes/fsmdef_states.h',
        './src/sipcc/core/includes/intelpentiumtypes.h',
        './src/sipcc/core/includes/kpml_common_util.h',
        './src/sipcc/core/includes/kpmlmap.h',
        './src/sipcc/core/includes/md5.h',
        './src/sipcc/core/includes/memory.h',
        './src/sipcc/core/includes/misc_apps_task.h',
        './src/sipcc/core/includes/misc_util.h',
        './src/sipcc/core/includes/phntask.h',
        './src/sipcc/core/includes/phone.h',
        './src/sipcc/core/includes/phone_debug.h',
        './src/sipcc/core/includes/phone_platform_constants.h',
        './src/sipcc/core/includes/phone_types.h',
        './src/sipcc/core/includes/platform_api.h',
        './src/sipcc/core/includes/pres_sub_not_handler.h',
        './src/sipcc/core/includes/publish_int.h',
        './src/sipcc/core/includes/regexp.h',
        './src/sipcc/core/includes/ringlist.h',
        './src/sipcc/core/includes/rtp_defs.h',
        './src/sipcc/core/includes/session.h',
        './src/sipcc/core/includes/sessionConstants.h',
        './src/sipcc/core/includes/sessionTypes.h',
        './src/sipcc/core/includes/sessuri.h',
        './src/sipcc/core/includes/singly_link_list.h',
        './src/sipcc/core/includes/sip_socket_api.h',
        './src/sipcc/core/includes/sntp.h',
        './src/sipcc/core/includes/string_lib.h',
        './src/sipcc/core/includes/subapi.h',
        './src/sipcc/core/includes/task.h',
        './src/sipcc/core/includes/time2.h',
        './src/sipcc/core/includes/timer.h',
        './src/sipcc/core/includes/tnpphone.h',
        './src/sipcc/core/includes/uart.h',
        './src/sipcc/core/includes/uiapi.h',
        './src/sipcc/core/includes/upgrade.h',
        './src/sipcc/core/includes/util_ios_queue.h',
        './src/sipcc/core/includes/util_parse.h',
        './src/sipcc/core/includes/util_string.h',
        './src/sipcc/core/includes/www.h',
        './src/sipcc/core/includes/xml_defs.h',
        # SDP
        './src/sipcc/core/sdp/ccsdp.c',
        './src/sipcc/core/sdp/sdp_access.c',
        './src/sipcc/core/sdp/sdp_attr.c',
        './src/sipcc/core/sdp/sdp_attr_access.c',
        './src/sipcc/core/sdp/sdp_base64.c',
        './src/sipcc/core/sdp/sdp_config.c',
        './src/sipcc/core/sdp/sdp_main.c',
        './src/sipcc/core/sdp/sdp_token.c',
        './src/sipcc/core/sdp/sdp.h',
        './src/sipcc/core/sdp/sdp_base64.h',
        './src/sipcc/core/sdp/sdp_os_defs.h',
        './src/sipcc/core/sdp/sdp_private.h',
        './src/sipcc/core/sdp/sdp_utils.c',
        './src/sipcc/core/sdp/sdp_services_unix.c',
        # SIPSTACK
        './src/sipcc/core/sipstack/ccsip_callinfo.c',
        './src/sipcc/core/sipstack/ccsip_cc.c',
        './src/sipcc/core/sipstack/ccsip_common_util.c',
        './src/sipcc/core/sipstack/ccsip_core.c',
        './src/sipcc/core/sipstack/ccsip_debug.c',
        './src/sipcc/core/sipstack/ccsip_info.c',
        './src/sipcc/core/sipstack/ccsip_messaging.c',
        './src/sipcc/core/sipstack/ccsip_platform.c',
        './src/sipcc/core/sipstack/ccsip_platform_tcp.c',
        './src/sipcc/core/sipstack/ccsip_platform_timers.c',
        './src/sipcc/core/sipstack/ccsip_platform_tls.c',
        './src/sipcc/core/sipstack/ccsip_platform_udp.c',
        './src/sipcc/core/sipstack/ccsip_pmh.c',
        './src/sipcc/core/sipstack/ccsip_publish.c',
        './src/sipcc/core/sipstack/ccsip_register.c',
        './src/sipcc/core/sipstack/ccsip_reldev.c',
        './src/sipcc/core/sipstack/ccsip_sdp.c',
        './src/sipcc/core/sipstack/ccsip_spi_utils.c',
        './src/sipcc/core/sipstack/ccsip_subsmanager.c',
        './src/sipcc/core/sipstack/ccsip_task.c',
        './src/sipcc/core/sipstack/httpish.c',
        './src/sipcc/core/sipstack/pmhutils.c',
        './src/sipcc/core/sipstack/sip_common_regmgr.c',
        './src/sipcc/core/sipstack/sip_common_transport.c',
        './src/sipcc/core/sipstack/sip_csps_transport.c',
        './src/sipcc/core/sipstack/sip_interface_regmgr.c',
        './src/sipcc/core/sipstack/h/ccsip_callinfo.h',
        './src/sipcc/core/sipstack/h/ccsip_cc.h',
        './src/sipcc/core/sipstack/h/ccsip_common_cb.h',
        './src/sipcc/core/sipstack/h/ccsip_core.h',
        './src/sipcc/core/sipstack/h/ccsip_credentials.h',
        './src/sipcc/core/sipstack/h/ccsip_macros.h',
        './src/sipcc/core/sipstack/h/ccsip_messaging.h',
        './src/sipcc/core/sipstack/h/ccsip_platform.h',
        './src/sipcc/core/sipstack/h/ccsip_platform_tcp.h',
        './src/sipcc/core/sipstack/h/ccsip_platform_timers.h',
        './src/sipcc/core/sipstack/h/ccsip_platform_tls.h',
        './src/sipcc/core/sipstack/h/ccsip_platform_udp.h',
        './src/sipcc/core/sipstack/h/ccsip_pmh.h',
        './src/sipcc/core/sipstack/h/ccsip_protocol.h',
        './src/sipcc/core/sipstack/h/ccsip_publish.h',
        './src/sipcc/core/sipstack/h/ccsip_register.h',
        './src/sipcc/core/sipstack/h/ccsip_reldev.h',
        './src/sipcc/core/sipstack/h/ccsip_sdp.h',
        './src/sipcc/core/sipstack/h/ccsip_sim.h',
        './src/sipcc/core/sipstack/h/ccsip_spi_utils.h',
        './src/sipcc/core/sipstack/h/ccsip_subsmanager.h',
        './src/sipcc/core/sipstack/h/ccsip_task.h',
        './src/sipcc/core/sipstack/h/httpish.h',
        './src/sipcc/core/sipstack/h/httpish_protocol.h',
        './src/sipcc/core/sipstack/h/pmhdefs.h',
        './src/sipcc/core/sipstack/h/pmhutils.h',
        './src/sipcc/core/sipstack/h/regmgrapi.h',
        './src/sipcc/core/sipstack/h/sip_ccm_transport.h',
        './src/sipcc/core/sipstack/h/sip_common_regmgr.h',
        './src/sipcc/core/sipstack/h/sip_common_transport.h',
        './src/sipcc/core/sipstack/h/sip_csps_transport.h',
        './src/sipcc/core/sipstack/h/sip_interface_regmgr.h',
        './src/sipcc/core/sipstack/h/sip_platform_task.h',
        # SRC-COMMON
        './src/sipcc/core/src-common/configapp.c',
        './src/sipcc/core/src-common/dialplan.c',
        './src/sipcc/core/src-common/dialplanint.c',
        './src/sipcc/core/src-common/digcalc.c',
        './src/sipcc/core/src-common/kpml_common_util.c',
        './src/sipcc/core/src-common/kpmlmap.c',
        './src/sipcc/core/src-common/md5.c',
        './src/sipcc/core/src-common/misc_apps_task.c',
        './src/sipcc/core/src-common/pres_sub_not_handler.c',
        './src/sipcc/core/src-common/publish_int.c',
        './src/sipcc/core/src-common/singly_link_list.c',
        './src/sipcc/core/src-common/sll_lite.c',
        './src/sipcc/core/src-common/string_lib.c',
        './src/sipcc/core/src-common/util_ios_queue.c',
        './src/sipcc/core/src-common/util_parse.c',
        './src/sipcc/core/src-common/util_string.c',
        # CPR
        './src/sipcc/cpr/include/cpr.h',
        './src/sipcc/cpr/include/cpr_assert.h',
        './src/sipcc/cpr/include/cpr_debug.h',
        './src/sipcc/cpr/include/cpr_errno.h',
        './src/sipcc/cpr/include/cpr_in.h',
        './src/sipcc/cpr/include/cpr_ipc.h',
        './src/sipcc/cpr/include/cpr_locks.h',
        './src/sipcc/cpr/include/cpr_memory.h',
        './src/sipcc/cpr/include/cpr_rand.h',
        './src/sipcc/cpr/include/cpr_socket.h',
        './src/sipcc/cpr/include/cpr_stddef.h',
        './src/sipcc/cpr/include/cpr_stdio.h',
        './src/sipcc/cpr/include/cpr_stdlib.h',
        './src/sipcc/cpr/include/cpr_string.h',
        './src/sipcc/cpr/include/cpr_strings.h',
        './src/sipcc/cpr/include/cpr_threads.h',
        './src/sipcc/cpr/include/cpr_time.h',
        './src/sipcc/cpr/include/cpr_timers.h',
        './src/sipcc/cpr/include/cpr_types.h',
        './src/sipcc/cpr/common/cpr_ipc.c',
        './src/sipcc/cpr/common/cpr_string.c',
        # INCLUDE
        './src/sipcc/include/cc_blf.h',
        './src/sipcc/include/cc_blf_listener.h',
        './src/sipcc/include/cc_call_feature.h',
        './src/sipcc/include/cc_call_listener.h',
        './src/sipcc/include/cc_config.h',
        './src/sipcc/include/cc_constants.h',
        './src/sipcc/include/cc_debug.h',
        './src/sipcc/include/cc_device_feature.h',
        './src/sipcc/include/cc_device_listener.h',
        './src/sipcc/include/cc_info.h',
        './src/sipcc/include/cc_info_listener.h',
        './src/sipcc/include/cc_service.h',
        './src/sipcc/include/cc_service_listener.h',
        './src/sipcc/include/cc_types.h',
        './src/sipcc/include/ccapi_call.h',
        './src/sipcc/include/ccapi_call_info.h',
        './src/sipcc/include/ccapi_call_listener.h',
        './src/sipcc/include/ccapi_calllog.h',
        './src/sipcc/include/ccapi_conf_roster.h',
        './src/sipcc/include/ccapi_device.h',
        './src/sipcc/include/ccapi_device_info.h',
        './src/sipcc/include/ccapi_device_listener.h',
        './src/sipcc/include/ccapi_feature_info.h',
        './src/sipcc/include/ccapi_line.h',
        './src/sipcc/include/ccapi_line_info.h',
        './src/sipcc/include/ccapi_line_listener.h',
        './src/sipcc/include/ccapi_service.h',
        './src/sipcc/include/ccapi_types.h',
        './src/sipcc/include/ccsdp.h',
        './src/sipcc/include/ccsdp_rtcp_fb.h',
        './src/sipcc/include/config_api.h',
        './src/sipcc/include/dns_util.h',
        './src/sipcc/include/plat_api.h',
        './src/sipcc/include/reset_api.h',
        './src/sipcc/include/sll_lite.h',
        './src/sipcc/include/vcm.h',
        './src/sipcc/include/xml_parser_defines.h',

        # PLAT
        './src/sipcc/plat/csf2g/model.c',
        './src/sipcc/plat/csf2g/reset_api.c',
        #
        # './src/sipcc/plat/common/plat_debug.h',
        # './src/sipcc/plat/common/tnp_blf.h',

        # STUB
        #'./src/sipcc/stub/cc_blf_stub.c',
        #'./src/sipcc/stub/vcm_stub.c',

      ],

      #
      # DEFINES
      #

      'defines' : [
      # CPR timers are needed by SIP, but are disabled for now
      # to avoid the extra timer thread and stale cleanup code
      #    'CPR_TIMERS_ENABLED',
      ],

      'cflags_mozilla': [
        '$(NSPR_CFLAGS)',
      ],

      #
      # OS SPECIFIC
      #
      'conditions': [
        ['(OS=="android") or (OS=="linux")', {
          'include_dirs': [
          ],

          'defines' : [
            'SIP_OS_LINUX',
            '_GNU_SOURCE',
            'CPR_MEMORY_LITTLE_ENDIAN',
            'NO_SOCKET_POLLING',
            'USE_TIMER_SELECT_BASED',
            'FULL_BUILD',
            'STUBBED_OUT',
            'USE_PRINTF'
            'LINUX',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['OS=="android"', {
          'sources': [
            # SIPSTACK
            './src/sipcc/core/sipstack/sip_platform_task.c',

            # PLAT
            './src/sipcc/plat/common/dns_utils.c',

            # CPR
            './src/sipcc/cpr/android/cpr_android_errno.c',
            './src/sipcc/cpr/android/cpr_android_init.c',
            './src/sipcc/cpr/android/cpr_android_socket.c',
            './src/sipcc/cpr/android/cpr_android_stdio.c',
            './src/sipcc/cpr/android/cpr_android_string.c',
            './src/sipcc/cpr/android/cpr_android_threads.c',
            './src/sipcc/cpr/android/cpr_android_timers_using_select.c',

            './src/sipcc/cpr/android/cpr_assert.h',
            './src/sipcc/cpr/android/cpr_android_align.h',
            './src/sipcc/cpr/android/cpr_android_assert.h',
            './src/sipcc/cpr/android/cpr_android_errno.h',
            './src/sipcc/cpr/android/cpr_android_in.h',
            './src/sipcc/cpr/android/cpr_android_private.h',
            './src/sipcc/cpr/android/cpr_android_rand.h',
            './src/sipcc/cpr/android/cpr_android_socket.h',
            './src/sipcc/cpr/android/cpr_android_stdio.h',
            './src/sipcc/cpr/android/cpr_android_string.h',
            './src/sipcc/cpr/android/cpr_android_strings.h',
            './src/sipcc/cpr/android/cpr_android_time.h',
            './src/sipcc/cpr/android/cpr_android_timers.h',
            './src/sipcc/cpr/android/cpr_android_tst.h',
            './src/sipcc/cpr/android/cpr_android_types.h',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            # SIPSTACK
            './src/sipcc/core/sipstack/sip_platform_task.c',

            # PLAT
            './src/sipcc/plat/common/dns_utils.c',

            # CPR
            './src/sipcc/cpr/linux/cpr_linux_errno.c',
            './src/sipcc/cpr/linux/cpr_linux_init.c',
            './src/sipcc/cpr/linux/cpr_linux_socket.c',
            './src/sipcc/cpr/linux/cpr_linux_stdio.c',
            './src/sipcc/cpr/linux/cpr_linux_string.c',
            './src/sipcc/cpr/linux/cpr_linux_threads.c',
            './src/sipcc/cpr/linux/cpr_linux_timers_using_select.c',

            './src/sipcc/cpr/linux/cpr_assert.h',
            './src/sipcc/cpr/linux/cpr_linux_align.h',
            './src/sipcc/cpr/linux/cpr_linux_assert.h',
            './src/sipcc/cpr/linux/cpr_linux_errno.h',
            './src/sipcc/cpr/linux/cpr_linux_in.h',
            './src/sipcc/cpr/linux/cpr_linux_private.h',
            './src/sipcc/cpr/linux/cpr_linux_rand.h',
            './src/sipcc/cpr/linux/cpr_linux_socket.h',
            './src/sipcc/cpr/linux/cpr_linux_stdio.h',
            './src/sipcc/cpr/linux/cpr_linux_string.h',
            './src/sipcc/cpr/linux/cpr_linux_strings.h',
            './src/sipcc/cpr/linux/cpr_linux_time.h',
            './src/sipcc/cpr/linux/cpr_linux_timers.h',
            './src/sipcc/cpr/linux/cpr_linux_tst.h',
            './src/sipcc/cpr/linux/cpr_linux_types.h',

          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
          ],

          'sources': [
            # SIPSTACK
            './src/sipcc/core/sipstack/sip_platform_win32_task.c',

            # PLAT
            './src/sipcc/plat/win32/dns_utils.c',
            './src/sipcc/plat/win32/mystub.c',
            './src/sipcc/plat/win32/plat_api_stub.c',
            './src/sipcc/plat/win32/plat_api_win.c',
            './src/sipcc/plat/win32/StdAfx.h',

            # CPR
            './src/sipcc/cpr/win32/cpr_win_assert.h',
            './src/sipcc/cpr/win32/cpr_win_debug.c',
            './src/sipcc/cpr/win32/cpr_win_debug.h',
            './src/sipcc/cpr/win32/cpr_win_defines.h',
            './src/sipcc/cpr/win32/cpr_win_errno.c',
            './src/sipcc/cpr/win32/cpr_win_errno.h',
            './src/sipcc/cpr/win32/cpr_win_in.h',
            './src/sipcc/cpr/win32/cpr_win_init.c',
            './src/sipcc/cpr/win32/cpr_win_locks.c',
            './src/sipcc/cpr/win32/cpr_win_locks.h',
            './src/sipcc/cpr/win32/cpr_win_rand.c',
            './src/sipcc/cpr/win32/cpr_win_rand.h',
            './src/sipcc/cpr/win32/cpr_win_socket.c',
            './src/sipcc/cpr/win32/cpr_win_socket.h',
            './src/sipcc/cpr/win32/cpr_win_stdio.c',
            './src/sipcc/cpr/win32/cpr_win_stdio.h',
            './src/sipcc/cpr/win32/cpr_win_string.c',
            './src/sipcc/cpr/win32/cpr_win_string.h',
            './src/sipcc/cpr/win32/cpr_win_strings.h',
            './src/sipcc/cpr/win32/cpr_win_threads.c',
            './src/sipcc/cpr/win32/cpr_win_time.h',
            './src/sipcc/cpr/win32/cpr_win_timers.c',
            './src/sipcc/cpr/win32/cpr_win_timers.h',
            './src/sipcc/cpr/win32/cpr_win_types.h',

          ],

          'defines' : [
            'SIP_OS_WINDOWS',
            'WIN32',
            'SIPCC_BUILD',
            'SDP_WIN32',
            'STUBBED_OUT',
            'EXTERNAL_TICK_REQUIRED',
            'GIPS_VER=3480',
          ],

          'cflags_mozilla': [
          ],

        }],
        ['OS=="mac" or os_bsd==1', {

          'include_dirs': [
          ],

          'sources': [
            # SIPSTACK
            './src/sipcc/core/sipstack/sip_platform_task.c',

            # PLAT
            './src/sipcc/plat/common/dns_utils.c',
            #'./src/sipcc/plat/darwin/netif.c',
            './src/sipcc/plat/darwin/plat_api_stub.c',
            #'./src/sipcc/plat/unix-common/random.c',

            # CPR
            './src/sipcc/cpr/darwin/cpr_darwin_assert.h',
            './src/sipcc/cpr/darwin/cpr_darwin_errno.c',
            './src/sipcc/cpr/darwin/cpr_darwin_errno.h',
            './src/sipcc/cpr/darwin/cpr_darwin_in.h',
            './src/sipcc/cpr/darwin/cpr_darwin_init.c',
            './src/sipcc/cpr/darwin/cpr_darwin_private.h',
            './src/sipcc/cpr/darwin/cpr_darwin_rand.h',
            './src/sipcc/cpr/darwin/cpr_darwin_socket.c',
            './src/sipcc/cpr/darwin/cpr_darwin_socket.h',
            './src/sipcc/cpr/darwin/cpr_darwin_stdio.c',
            './src/sipcc/cpr/darwin/cpr_darwin_stdio.h',
            './src/sipcc/cpr/darwin/cpr_darwin_string.c',
            './src/sipcc/cpr/darwin/cpr_darwin_string.h',
            './src/sipcc/cpr/darwin/cpr_darwin_strings.h',
            './src/sipcc/cpr/darwin/cpr_darwin_threads.c',
            './src/sipcc/cpr/darwin/cpr_darwin_time.h',
            './src/sipcc/cpr/darwin/cpr_darwin_timers.h',
            './src/sipcc/cpr/darwin/cpr_darwin_timers_using_select.c',
            './src/sipcc/cpr/darwin/cpr_darwin_tst.h',
            './src/sipcc/cpr/darwin/cpr_darwin_types.h',
          ],


          'conditions': [
            ['OS=="mac"', {
              'defines' : [
                'SIP_OS_OSX',
                '_POSIX_SOURCE',
                'CPR_MEMORY_LITTLE_ENDIAN',
                'NO_SOCKET_POLLING',
                'USE_TIMER_SELECT_BASED',
                'FULL_BUILD',
                'STUBBED_OUT',
                'USE_PRINTF',
                '_DARWIN_C_SOURCE',
                'NO_NSPR_10_SUPPORT',
              ],
            }],
            ['os_bsd==1', {
              'defines' : [
                'SIP_OS_OSX',
                'CPR_MEMORY_LITTLE_ENDIAN',
                'NO_SOCKET_POLLING',
                'USE_TIMER_SELECT_BASED',
                'FULL_BUILD',
                'STUBBED_OUT',
                'USE_PRINTF',
                'NO_NSPR_10_SUPPORT',
              ],
            }],
          ],
          'cflags_mozilla': [
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
