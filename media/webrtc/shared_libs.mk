# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# shared libs for webrtc
WEBRTC_LIBS = \
  $(call EXPAND_LIBNAME_PATH,common_video,$(DEPTH)/media/webrtc/trunk/webrtc/common_video/common_video_common_video) \
  $(call EXPAND_LIBNAME_PATH,video_capture_module,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_video_capture_module) \
  $(call EXPAND_LIBNAME_PATH,webrtc_utility,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_webrtc_utility) \
  $(call EXPAND_LIBNAME_PATH,audio_coding_module,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_audio_coding_module) \
  $(call EXPAND_LIBNAME_PATH,CNG,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_CNG) \
  $(call EXPAND_LIBNAME_PATH,signal_processing,$(DEPTH)/media/webrtc/trunk/webrtc/common_audio/common_audio_signal_processing) \
  $(call EXPAND_LIBNAME_PATH,G711,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_G711) \
  $(call EXPAND_LIBNAME_PATH,PCM16B,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_PCM16B) \
  $(call EXPAND_LIBNAME_PATH,NetEq,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_NetEq) \
  $(call EXPAND_LIBNAME_PATH,resampler,$(DEPTH)/media/webrtc/trunk/webrtc/common_audio/common_audio_resampler) \
  $(call EXPAND_LIBNAME_PATH,vad,$(DEPTH)/media/webrtc/trunk/webrtc/common_audio/common_audio_vad) \
  $(call EXPAND_LIBNAME_PATH,system_wrappers,$(DEPTH)/media/webrtc/trunk/webrtc/system_wrappers/source/system_wrappers_system_wrappers) \
  $(call EXPAND_LIBNAME_PATH,webrtc_video_coding,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_webrtc_video_coding) \
  $(call EXPAND_LIBNAME_PATH,webrtc_i420,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_webrtc_i420) \
  $(call EXPAND_LIBNAME_PATH,webrtc_vp8,$(DEPTH)/media/webrtc/trunk/webrtc/modules/video_coding/codecs/vp8/vp8_webrtc_vp8) \
  $(call EXPAND_LIBNAME_PATH,webrtc_opus,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_webrtc_opus) \
  $(call EXPAND_LIBNAME_PATH,video_render_module,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_video_render_module) \
  $(call EXPAND_LIBNAME_PATH,video_engine_core,$(DEPTH)/media/webrtc/trunk/webrtc/video_engine/video_engine_video_engine_core) \
  $(call EXPAND_LIBNAME_PATH,media_file,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_media_file) \
  $(call EXPAND_LIBNAME_PATH,rtp_rtcp,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_rtp_rtcp) \
  $(call EXPAND_LIBNAME_PATH,udp_transport,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_udp_transport) \
  $(call EXPAND_LIBNAME_PATH,bitrate_controller,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_bitrate_controller) \
  $(call EXPAND_LIBNAME_PATH,remote_bitrate_estimator,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_remote_bitrate_estimator) \
  $(call EXPAND_LIBNAME_PATH,paced_sender,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_paced_sender) \
  $(call EXPAND_LIBNAME_PATH,video_processing,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_video_processing) \
  $(call EXPAND_LIBNAME_PATH,voice_engine_core,$(DEPTH)/media/webrtc/trunk/webrtc/voice_engine/voice_engine_voice_engine_core) \
  $(call EXPAND_LIBNAME_PATH,audio_conference_mixer,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_audio_conference_mixer) \
  $(call EXPAND_LIBNAME_PATH,audio_device,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_audio_device) \
  $(call EXPAND_LIBNAME_PATH,audio_processing,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_audio_processing) \
  $(call EXPAND_LIBNAME_PATH,yuv,$(DEPTH)/media/webrtc/trunk/third_party/libyuv/libyuv_libyuv) \
  $(call EXPAND_LIBNAME_PATH,nicer,$(DEPTH)/media/mtransport/third_party/nICEr/nicer_nicer) \
  $(call EXPAND_LIBNAME_PATH,nrappkit,$(DEPTH)/media/mtransport/third_party/nrappkit/nrappkit_nrappkit) \
  $(NULL)

# if we're on an intel arch, we want SSE2 optimizations
ifneq (,$(INTEL_ARCHITECTURE))
WEBRTC_LIBS += \
  $(call EXPAND_LIBNAME_PATH,video_processing_sse2,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_video_processing_sse2) \
  $(call EXPAND_LIBNAME_PATH,audio_processing_sse2,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_audio_processing_sse2) \
  $(NULL)
endif

ifeq ($(CPU_ARCH), arm)
ifeq (Android,$(OS_TARGET))
# NEON detection on WebRTC is Android only. If WebRTC supports Linux/arm etc,
# we should remove OS check
# extra ARM libs
WEBRTC_LIBS += \
  $(call EXPAND_LIBNAME_PATH,cpu_features_android,$(DEPTH)/media/webrtc/trunk/webrtc/system_wrappers/source/system_wrappers_cpu_features_android) \
  $(NULL)
# neon for ARM
ifeq ($(BUILD_ARM_NEON),1)
WEBRTC_LIBS += \
  $(call EXPAND_LIBNAME_PATH,signal_processing_neon,$(DEPTH)/media/webrtc/trunk/webrtc/common_audio/common_audio_signal_processing_neon) \
  $(call EXPAND_LIBNAME_PATH,audio_processing_neon,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_audio_processing_neon) \
  $(NULL)
endif
endif
endif


# If you enable one of these codecs in webrtc_config.gypi, you'll need to re-add the
# relevant library from this list:
#
#  $(call EXPAND_LIBNAME_PATH,G722,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_G722) \
#  $(call EXPAND_LIBNAME_PATH,iLBC,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_iLBC) \
#  $(call EXPAND_LIBNAME_PATH,iSAC,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_iSAC) \
#  $(call EXPAND_LIBNAME_PATH,iSACFix,$(DEPTH)/media/webrtc/trunk/webrtc/modules/modules_iSACFix) \
#
