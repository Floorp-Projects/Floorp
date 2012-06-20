# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# shared libs for webrtc
SHARED_LIBRARY_LIBS += \
  $(call EXPAND_LIBNAME_PATH,video_capture_module,$(DEPTH)/media/webrtc/trunk/src/modules/modules_video_capture_module) \
  $(call EXPAND_LIBNAME_PATH,webrtc_utility,$(DEPTH)/media/webrtc/trunk/src/modules/modules_webrtc_utility) \
  $(call EXPAND_LIBNAME_PATH,audio_coding_module,$(DEPTH)/media/webrtc/trunk/src/modules/modules_audio_coding_module) \
  $(call EXPAND_LIBNAME_PATH,CNG,$(DEPTH)/media/webrtc/trunk/src/modules/modules_CNG) \
  $(call EXPAND_LIBNAME_PATH,signal_processing,$(DEPTH)/media/webrtc/trunk/src/common_audio/common_audio_signal_processing) \
  $(call EXPAND_LIBNAME_PATH,G711,$(DEPTH)/media/webrtc/trunk/src/modules/modules_G711) \
  $(call EXPAND_LIBNAME_PATH,G722,$(DEPTH)/media/webrtc/trunk/src/modules/modules_G722) \
  $(call EXPAND_LIBNAME_PATH,iLBC,$(DEPTH)/media/webrtc/trunk/src/modules/modules_iLBC) \
  $(call EXPAND_LIBNAME_PATH,iSAC,$(DEPTH)/media/webrtc/trunk/src/modules/modules_iSAC) \
  $(call EXPAND_LIBNAME_PATH,iSACFix,$(DEPTH)/media/webrtc/trunk/src/modules/modules_iSACFix) \
  $(call EXPAND_LIBNAME_PATH,PCM16B,$(DEPTH)/media/webrtc/trunk/src/modules/modules_PCM16B) \
  $(call EXPAND_LIBNAME_PATH,NetEq,$(DEPTH)/media/webrtc/trunk/src/modules/modules_NetEq) \
  $(call EXPAND_LIBNAME_PATH,resampler,$(DEPTH)/media/webrtc/trunk/src/common_audio/common_audio_resampler) \
  $(call EXPAND_LIBNAME_PATH,vad,$(DEPTH)/media/webrtc/trunk/src/common_audio/common_audio_vad) \
  $(call EXPAND_LIBNAME_PATH,system_wrappers,$(DEPTH)/media/webrtc/trunk/src/system_wrappers/source/system_wrappers_system_wrappers) \
  $(call EXPAND_LIBNAME_PATH,webrtc_video_coding,$(DEPTH)/media/webrtc/trunk/src/modules/modules_webrtc_video_coding) \
  $(call EXPAND_LIBNAME_PATH,webrtc_i420,$(DEPTH)/media/webrtc/trunk/src/modules/modules_webrtc_i420) \
  $(call EXPAND_LIBNAME_PATH,webrtc_vp8,$(DEPTH)/media/webrtc/trunk/src/modules/modules_webrtc_vp8) \
  $(call EXPAND_LIBNAME_PATH,webrtc_libyuv,$(DEPTH)/media/webrtc/trunk/src/common_video/common_video_webrtc_libyuv) \
  $(call EXPAND_LIBNAME_PATH,video_render_module,$(DEPTH)/media/webrtc/trunk/src/modules/modules_video_render_module) \
  $(call EXPAND_LIBNAME_PATH,video_engine_core,$(DEPTH)/media/webrtc/trunk/src/video_engine/video_engine_video_engine_core) \
  $(call EXPAND_LIBNAME_PATH,media_file,$(DEPTH)/media/webrtc/trunk/src/modules/modules_media_file) \
  $(call EXPAND_LIBNAME_PATH,rtp_rtcp,$(DEPTH)/media/webrtc/trunk/src/modules/modules_rtp_rtcp) \
  $(call EXPAND_LIBNAME_PATH,udp_transport,$(DEPTH)/media/webrtc/trunk/src/modules/modules_udp_transport) \
  $(call EXPAND_LIBNAME_PATH,video_processing,$(DEPTH)/media/webrtc/trunk/src/modules/modules_video_processing) \
  $(call EXPAND_LIBNAME_PATH,video_processing_sse2,$(DEPTH)/media/webrtc/trunk/src/modules/modules_video_processing_sse2) \
  $(call EXPAND_LIBNAME_PATH,voice_engine_core,$(DEPTH)/media/webrtc/trunk/src/voice_engine/voice_engine_voice_engine_core) \
  $(call EXPAND_LIBNAME_PATH,audio_conference_mixer,$(DEPTH)/media/webrtc/trunk/src/modules/modules_audio_conference_mixer) \
  $(call EXPAND_LIBNAME_PATH,audio_device,$(DEPTH)/media/webrtc/trunk/src/modules/modules_audio_device) \
  $(call EXPAND_LIBNAME_PATH,audio_processing,$(DEPTH)/media/webrtc/trunk/src/modules/modules_audio_processing) \
  $(call EXPAND_LIBNAME_PATH,aec,$(DEPTH)/media/webrtc/trunk/src/modules/modules_aec) \
  $(call EXPAND_LIBNAME_PATH,aec_sse2,$(DEPTH)/media/webrtc/trunk/src/modules/modules_aec_sse2) \
  $(call EXPAND_LIBNAME_PATH,apm_util,$(DEPTH)/media/webrtc/trunk/src/modules/modules_apm_util) \
  $(call EXPAND_LIBNAME_PATH,aecm,$(DEPTH)/media/webrtc/trunk/src/modules/modules_aecm) \
  $(call EXPAND_LIBNAME_PATH,agc,$(DEPTH)/media/webrtc/trunk/src/modules/modules_agc) \
  $(call EXPAND_LIBNAME_PATH,ns,$(DEPTH)/media/webrtc/trunk/src/modules/modules_ns) \
  $(call EXPAND_LIBNAME_PATH,yuv,$(DEPTH)/media/webrtc/trunk/third_party/libyuv/libyuv_libyuv) \
  $(call EXPAND_LIBNAME_PATH,webrtc_jpeg,$(DEPTH)/media/webrtc/trunk/src/common_video/common_video_webrtc_jpeg) \
  $(NULL)
