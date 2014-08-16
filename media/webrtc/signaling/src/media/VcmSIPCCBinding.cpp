/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"

#include "CC_Common.h"

#include "CSFMediaProvider.h"
#include "CSFAudioTermination.h"
#include "CSFVideoTermination.h"
#include "MediaConduitErrors.h"
#include "MediaConduitInterface.h"
#include "GmpVideoCodec.h"
#include "MediaPipeline.h"
#include "MediaPipelineFilter.h"
#include "VcmSIPCCBinding.h"
#include "csf_common.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "nsThreadUtils.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"
#include "runnable_utils.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsIPrincipal.h"
#include "nsIDocument.h"
#include "mozilla/Preferences.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ssl.h>
#include <sslproto.h>
#include <algorithm>

#ifdef MOZ_WEBRTC_OMX
#include "OMXVideoCodec.h"
#include "OMXCodecWrapper.h"
#endif

extern "C" {
#include "ccsdp.h"
#include "vcm.h"
#include "cc_call_feature.h"
#include "cip_mmgr_mediadefinitions.h"
#include "cip_Sipcc_CodecMask.h"

extern void lsm_start_multipart_tone_timer (vcm_tones_t tone,
                                            uint32_t delay,
                                            cc_call_handle_t callId);
extern void lsm_start_continuous_tone_timer (vcm_tones_t tone,
                                             uint32_t delay,
                                             cc_call_handle_t callId);
extern void lsm_update_active_tone(vcm_tones_t tone, cc_call_handle_t call_handle);
extern void lsm_stop_multipart_tone_timer(void);
extern void lsm_stop_continuous_tone_timer(void);

static int vcmEnsureExternalCodec(
    const mozilla::RefPtr<mozilla::VideoSessionConduit>& conduit,
    mozilla::VideoCodecConfig* config,
    bool send);

}//end extern "C"

static const char* logTag = "VcmSipccBinding";

// Cloned from ccapi.h
typedef enum {
    CC_AUDIO_1,
    CC_VIDEO_1,
    CC_DATACHANNEL_1
} cc_media_cap_name;

#define SIPSDP_ILBC_MODE20 20

/* static */

using namespace mozilla;
using namespace CSF;

VcmSIPCCBinding * VcmSIPCCBinding::gSelf = nullptr;
bool VcmSIPCCBinding::gInitGmpCodecs = false;
int VcmSIPCCBinding::gAudioCodecMask = 0;
int VcmSIPCCBinding::gVideoCodecMask = 0;
int VcmSIPCCBinding::gVideoCodecGmpMask = 0;
nsIThread *VcmSIPCCBinding::gMainThread = nullptr;
nsIEventTarget *VcmSIPCCBinding::gSTSThread = nullptr;
nsCOMPtr<nsIPrefBranch> VcmSIPCCBinding::gBranch = nullptr;

static mozilla::RefPtr<TransportFlow> vcmCreateTransportFlow(
    sipcc::PeerConnectionImpl *pc,
    int level,
    bool rtcp,
    sdp_setup_type_e setup_type,
    const char *fingerprint_alg,
    const char *fingerprint);

// Convenience macro to acquire PC

#define ENSURE_PC(pc, errval) \
  do { \
    if (!pc.impl()) {                                                 \
      CSFLogDebug(logTag, "%s: couldn't acquire peerconnection %s", __FUNCTION__, peerconnection); \
      return errval; \
    }         \
  } while(0)

#define ENSURE_PC_NO_RET(pc, peerconnection) \
  do { \
    if (!pc.impl()) {                                                 \
      CSFLogDebug(logTag, "%s: couldn't acquire peerconnection %s", __FUNCTION__, peerconnection); \
      return; \
    }         \
  } while(0)

VcmSIPCCBinding::VcmSIPCCBinding ()
  : streamObserver(nullptr)
{
    delete gSelf;
    gSelf = this;
  nsresult rv;

  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    gBranch = do_QueryInterface(prefs);
  }
}

class VcmIceOpaque : public NrIceOpaque {
 public:
  VcmIceOpaque(cc_call_handle_t call_handle,
               uint16_t level) :
      call_handle_(call_handle),
      level_(level) {}

  virtual ~VcmIceOpaque() {}

  cc_call_handle_t call_handle_;
  uint16_t level_;
};


VcmSIPCCBinding::~VcmSIPCCBinding ()
{
    assert(gSelf);
    gSelf = nullptr;
    // In case we're torn down while STS is still running,
    // we try to dispatch to STS to disconnect all of the
    // ICE signals. If STS is no longer running, this will
    // harmlessly fail.
    SyncRunnable::DispatchToThread(
      gSTSThread,
      WrapRunnable(this, &VcmSIPCCBinding::disconnect_all),
      true);

  gBranch = nullptr;
}

void VcmSIPCCBinding::CandidateReady(NrIceMediaStream* stream,
                                     const std::string& candidate)
{
    // This is called on the STS thread
    NrIceOpaque *opaque = stream->opaque();
    MOZ_ASSERT(opaque);

    VcmIceOpaque *vcm_opaque = static_cast<VcmIceOpaque *>(opaque);
    CSFLogDebug(logTag, "Candidate ready on call %u, level %u: %s",
                vcm_opaque->call_handle_, vcm_opaque->level_, candidate.c_str());

    char *candidate_tmp = (char *)malloc(candidate.size() + 1);
    if (!candidate_tmp)
        return;
    sstrncpy(candidate_tmp, candidate.c_str(), candidate.size() + 1);
    // Send a message to the GSM thread.
    CC_CallFeature_FoundICECandidate(vcm_opaque->call_handle_,
                                     candidate_tmp,
                                     nullptr,
                                     vcm_opaque->level_,
                                     nullptr);
}

void VcmSIPCCBinding::setStreamObserver(StreamObserver* obs)
{
        streamObserver = obs;
}

/* static */
StreamObserver * VcmSIPCCBinding::getStreamObserver()
{
    if (gSelf != nullptr)
        return gSelf->streamObserver;

    return nullptr;
}

void VcmSIPCCBinding::setMediaProviderObserver(MediaProviderObserver* obs)
{
        mediaProviderObserver = obs;
}


MediaProviderObserver * VcmSIPCCBinding::getMediaProviderObserver()
{
    if (gSelf != nullptr)
        return gSelf->mediaProviderObserver;

    return nullptr;
}

void VcmSIPCCBinding::setAudioCodecs(int codecMask)
{
  CSFLogDebug(logTag, "SETTING AUDIO: %d", codecMask);
  VcmSIPCCBinding::gAudioCodecMask = codecMask;
}

void VcmSIPCCBinding::setVideoCodecs(int codecMask)
{
  CSFLogDebug(logTag, "SETTING VIDEO: %d", codecMask);
  VcmSIPCCBinding::gVideoCodecMask = codecMask;
}

int VcmSIPCCBinding::getAudioCodecs()
{
  return VcmSIPCCBinding::gAudioCodecMask;
}

int VcmSIPCCBinding::getVideoCodecs()
{
  return VcmSIPCCBinding::gVideoCodecMask;
}

static void GMPDummy() {};

bool VcmSIPCCBinding::scanForGmpCodecs()
{
  if (!gSelf) {
    return false;
  }
  if (!gSelf->mGMPService) {
    gSelf->mGMPService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    if (!gSelf->mGMPService) {
      return false;
    }
  }

  // XXX find a way to a) do this earlier, b) not block mainthread
  // Perhaps fire on first RTCPeerconnection creation, and block
  // processing (async) CreateOffer or CreateAnswer's until it has returned.
  // Since they're already async, it's easy to avoid starting them there.
  // However, we might like to do it even earlier, perhaps.

  // XXX We shouldn't be blocking MainThread on the GMP thread!
  // This initiates the scan for codecs
  nsCOMPtr<nsIThread> thread;
  nsresult rv = gSelf->mGMPService->GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return false;
  }
  // presumes that all GMP dir scans have been queued for the GMPThread
  mozilla::SyncRunnable::DispatchToThread(thread,
                                          WrapRunnableNM(&GMPDummy));
  return true;
}

int VcmSIPCCBinding::getVideoCodecsGmp()
{
  if (!gInitGmpCodecs) {
    if (scanForGmpCodecs()) {
      gInitGmpCodecs = true;
    }
  }
  if (gInitGmpCodecs) {
    if (!gSelf->mGMPService) {
     gSelf->mGMPService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    }
    if (gSelf->mGMPService) {
      // XXX I'd prefer if this was all known ahead of time...

      nsTArray<nsCString> tags;
      tags.AppendElement(NS_LITERAL_CSTRING("h264"));

      // H.264 only for now
      bool has_gmp;
      nsresult rv;
      rv = gSelf->mGMPService->HasPluginForAPI(NS_LITERAL_STRING(""),
                                               NS_LITERAL_CSTRING("encode-video"),
                                               &tags,
                                               &has_gmp);
      if (NS_SUCCEEDED(rv) && has_gmp) {
        rv = gSelf->mGMPService->HasPluginForAPI(NS_LITERAL_STRING(""),
                                                 NS_LITERAL_CSTRING("decode-video"),
                                                 &tags,
                                                 &has_gmp);
        if (NS_SUCCEEDED(rv) && has_gmp) {
          return VCM_CODEC_RESOURCE_H264;
        }
      }
    }
  }
  return 0;
}

int VcmSIPCCBinding::getVideoCodecsHw()
{
  // Check to see if what HW codecs are available (not in use) at this moment.
  // Note that streaming video decode can reserve a decoder

  // XXX See bug 1018791 Implement W3 codec reservation policy
  // Note that currently, OMXCodecReservation needs to be held by an sp<> because it puts
  // 'this' into an sp<EventListener> to talk to the resource reservation code
#ifdef MOZ_WEBRTC_OMX
#ifdef MOZILLA_INTERNAL_API
  if (Preferences::GetBool("media.peerconnection.video.h264_enabled")) {
#endif
    android::sp<android::OMXCodecReservation> encode = new android::OMXCodecReservation(true);
    android::sp<android::OMXCodecReservation> decode = new android::OMXCodecReservation(false);

    // Currently we just check if they're available right now, which will fail if we're
    // trying to call ourself, for example.  It will work for most real-world cases, like
    // if we try to add a person to a 2-way call to make a 3-way mesh call
    if (encode->ReserveOMXCodec() && decode->ReserveOMXCodec()) {
      CSFLogDebug( logTag, "%s: H264 hardware codec available", __FUNCTION__);
      return VCM_CODEC_RESOURCE_H264;
    }
#if defined( MOZILLA_INTERNAL_API)
   }
#endif
#endif

  return 0;
}

void VcmSIPCCBinding::setMainThread(nsIThread *thread)
{
  gMainThread = thread;
}

void VcmSIPCCBinding::setSTSThread(nsIEventTarget *thread)
{
  gSTSThread = thread;
}

nsIThread* VcmSIPCCBinding::getMainThread()
{
  return gMainThread;
}

nsIEventTarget* VcmSIPCCBinding::getSTSThread()
{
  return gSTSThread;
}

void VcmSIPCCBinding::connectCandidateSignal(
    NrIceMediaStream *stream)
{
  stream->SignalCandidate.connect(gSelf,
                                  &VcmSIPCCBinding::CandidateReady);
}

nsCOMPtr<nsIPrefBranch> VcmSIPCCBinding::getPrefBranch()
{
  return gBranch;
}

/* static */
AudioTermination * VcmSIPCCBinding::getAudioTermination()
{
    // commenting as part of media provider removal
    return nullptr;
}

/* static */
VideoTermination * VcmSIPCCBinding::getVideoTermination()
{
    // commenting as part of media provider removal
    return nullptr;
}

/* static */
AudioControl * VcmSIPCCBinding::getAudioControl()
{
    // commenting as part of media provider removal
    return nullptr;
}

/* static */
VideoControl * VcmSIPCCBinding::getVideoControl()
{
    // commenting as part of media provider removal
    return nullptr;
}

/*
 * Used to play busy verfication tone
 */
#define BUSY_VERIFICATION_DELAY       (10000)

/*
 * Temp until added to config
 */

#define TOH_DELAY                (10000)/*
 * Used to play msg waiting and stutter dialtones.
 * Both tones are 100ms on/off repeating 10 and
 * 3 times respectively, followed by steady dialtone.
 * Due to DSP limitations we first tell the DSP to
 * play the 100ms on/off pairs the correct number of
 * times, set a timer, and then tell it to play dialtone.
 */
#define MSG_WAITING_DELAY   (2050)
#define STUTTER_DELAY       (650)

/*
 * Used to play the busy verfication tone which
 * is two seconds of dialtone followed by the
 * callwaiting tone every ten seconds.
 */
#define BUSY_VERIFY_DELAY   (12000)


#define VCM_MIN_VOLUME_BYTE       0
#define VCM_MAX_VOLUME_BYTE       255
#define VCM_MIN_VOLUME_LEVEL      0
#define VCM_MAX_VOLUME_LEVEL      240
#define VCM_RNG_MAX_VOLUME_LEVEL  248
#define VCM_DEFAULT_AUDIO_VOLUME  144
#define VCM_DEFAULT_RINGER_VOLUME 80
#define VCM_VOLUME_ADJUST_LEVEL   8


extern "C" {


/* MediaTermination APIs expect dynmic payload type in upper 16 bits and
   codec type in lower 16 bits. */
#define CREATE_MT_MAP(a,b)        ((a << 16) | b)
#define DYNAMIC_PAYLOAD_TYPE(x)    ((x >> 16) & 0xFFFF)

#define VCM_ERROR -1

/**
 *  start/stop ringing
 *
 *  @param[in] ringMode   - VCM ring mode (ON/OFF)
 *  @param[in] once       - type of ring - continuous or one shot.
 *  @param[in] alert_info - header specified ring mode.
 *  @param[in] line       - the line on which to start/stop ringing
 *
 *  @return    void
 */

void vcmControlRinger (vcm_ring_mode_t ringMode,
                       short once,
                       cc_boolean alert_info,
                       int line,
                       cc_callid_t call_id)
{
    const char fname[] = "vcmControlRinger";

    CSFLogDebug( logTag, "%s: ringMode=%d once=%d", fname, ringMode, once);

    /* we need to pass the line parameter for this */

#if 0
     // <emannion> part of media provider removal
    if ( ringMode == VCM_RING_OFF )
    {
        //call media_ring_stop
        if ( VcmSIPCCBinding::getAudioTermination()->ringStop( line ) != 0 )
        {
            CSFLogDebug( logTag, "%s: mediaRingStop failed", fname);
        }
    }
    else if ( VcmSIPCCBinding::getAudioTermination()->ringStart( line, map_ring_mode(ringMode), (once != 0) ) != 0 )
    {
        CSFLogDebug( logTag, "%s: mediaRingStart failed", fname);
    }
#endif
}

/**
 *  To be Deprecated
 *  This may be needed to be implemented if the DSP doesn't automatically enable the side tone
 *  The stack will make a call to this method based on the call state. Provide a stub if this is not needed.
 *
 *  @param[in] side_tone - cc_boolean to enable/disable side tone
 *
 *  @return void
 *
 */

void vcmEnableSidetone(cc_uint16_t side_tone)
{
    /* NOT REQD for TNP */
    CSFLogDebug( logTag, "vcmEnableSidetone: vcmEnableSidetone(): called");
}

/*
 *  Function:map_algorithmID
 *
 *  Parameters:algorithmID
 *
 *  Description: Converts VCM algorithmID type to media layer defs.
 *
 *  Returns:algorithmID value corresponding to VCM algorithmID
 *
 */
static EncryptionAlgorithm
map_algorithmID( vcm_crypto_algorithmID algorithmID )
{
    switch ( algorithmID )
    {
    case VCM_AES_128_COUNTER:
        return EncryptionAlgorithm_AES_128_COUNTER;

    default:
        return EncryptionAlgorithm_NONE;
    }
}

/*
 *  Function:vcm_init
 *
 *  Parameters:none
 *
 *  Description: Nothing to do for TNP.
 *
 *  Returns:none
 *
 */
void vcmInit (void)
{
    CSFLogDebug( logTag, "vcmInit() called.");
}

/**
 * The unloading of the VCM module
 */
void vcmUnload (void)
{
    CSFLogDebug( logTag, "vcmUnload() called.");
}

/**
 *  Allocate(Reserve) a receive port.
 *
 *  @param[in]  mcap_id - Media Capability ID
 *  @param[in]  group_id - group identifier to which stream belongs.
 *  @param[in]  stream_id - stream identifier
 *  @param[in]  call_handle  - call identifier
 *  @param[in]  port_requested - port requested (if zero -> give any)
 *  @param[out]  port_allocated - port that was actually allocated.
 *
 *  @return    void
 *
 */

void vcmRxAllocPort(cc_mcapid_t mcap_id,
                    cc_groupid_t group_id,
                    cc_streamid_t stream_id,
                    cc_call_handle_t  call_handle,
                    cc_uint16_t port_requested,
                    int *port_allocated)
{
    *port_allocated = -1;
    CSFLogDebug( logTag, "vcmRxAllocPort(): group_id=%d stream_id=%d call_handle=%d port_requested = %d",
        group_id, stream_id, call_handle, port_requested);

    // Not in SDP/PeerConnection mode
    int port = -1;
    bool isVideo = false;
    if(CC_IS_AUDIO(mcap_id))
    {
      isVideo = false;
      if ( VcmSIPCCBinding::getAudioTermination() != nullptr )
        port = VcmSIPCCBinding::getAudioTermination()->rxAlloc( group_id, stream_id, port_requested );
    }
    else if(CC_IS_VIDEO(mcap_id))
    {
      isVideo = true;
      if ( VcmSIPCCBinding::getVideoTermination() != nullptr )
        port = VcmSIPCCBinding::getVideoTermination()->rxAlloc( group_id, stream_id, port_requested );
    }

    StreamObserver* obs = VcmSIPCCBinding::getStreamObserver();
    if(obs != nullptr)
      obs->registerStream(call_handle, stream_id, isVideo);

    CSFLogDebug( logTag, "vcmRxAllocPort(): allocated port %d", port);
    *port_allocated = port;
}


/**
 *  Gets the ICE objects for a stream.
 *
 *  @param[in]  mcap_id - Media Capability ID
 *  @param[in]  group_id - group identifier to which stream belongs.
 *  @param[in]  stream_id - stream identifier
 *  @param[in]  call_handle  - call identifier
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  level - the m-line index (1-based)
 *  @param[out] ctx - the NrIceCtx
 *  @param[out] stream - the NrIceStream
 */

static short vcmGetIceStream_m(cc_mcapid_t mcap_id,
                               cc_groupid_t group_id,
                               cc_streamid_t stream_id,
                               cc_call_handle_t  call_handle,
                               const char *peerconnection,
                               uint16_t level,
                               mozilla::RefPtr<NrIceCtx> *ctx,
                               mozilla::RefPtr<NrIceMediaStream> *stream)
{
  CSFLogDebug( logTag, "%s: group_id=%d stream_id=%d call_handle=%d PC = %s",
    __FUNCTION__, group_id, stream_id, call_handle, peerconnection);

  // Note: we don't acquire any media resources here, and we assume that the
  // ICE streams already exist, so we're just acquiring them. Any logic
  // to make them on demand is elsewhere.
  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  *ctx = pc.impl()->media()->ice_ctx();
  MOZ_ASSERT(*ctx);
  if (!*ctx)
    return VCM_ERROR;

  CSFLogDebug( logTag, "%s: Getting stream %d", __FUNCTION__, level);
  *stream = pc.impl()->media()->ice_media_stream(level-1);
  MOZ_ASSERT(*stream);
  if (!*stream) {
    return VCM_ERROR;
  }

  return 0;
}

/**
 *  Gets the ICE parameters for a stream. Called "alloc" for style consistency
 *  @param[in]  ctx_in - the ICE ctx
 *  @param[in]  stream_in - the ICE stream
 *  @param[in]  call_handle  - call identifier
 *  @param[in]  stream_id - stream identifier
 *  @param[in]  level - the m-line index (1-based)
 *  @param[out] default_addrp - the ICE default addr
 *  @param[out] port_allocatedp - the ICE default port
 *  @param[out] candidatesp - the ICE candidate array
 *  @param[out] candidate_ctp length of the array
 *
 *  @return 0 for success; VCM_ERROR for failure
 *
 */
static short vcmRxAllocICE_s(TemporaryRef<NrIceCtx> ctx_in,
                             TemporaryRef<NrIceMediaStream> stream_in,
                             cc_call_handle_t  call_handle,
                             cc_streamid_t stream_id,
                             uint16_t level,
                             char **default_addrp, /* Out */
                             int *default_portp, /* Out */
                             char ***candidatesp, /* Out */
                             int *candidate_ctp /* Out */
)
{
  // Make a concrete reference to ctx_in and stream_in so we
  // can use the pointers (TemporaryRef is not dereferencable).
  RefPtr<NrIceCtx> ctx(ctx_in);
  RefPtr<NrIceMediaStream> stream(stream_in);

  *default_addrp = nullptr;
  *default_portp = -1;
  *candidatesp = nullptr;
  *candidate_ctp = 0;

  // This can be called multiple times; don't connect to the signal more than
  // once (see bug 1018473 for an explanation).
  if (!stream->opaque()) {
    // Set the opaque so we can correlate events.
    stream->SetOpaque(new VcmIceOpaque(call_handle, level));

    // Attach ourself to the candidate signal.
    VcmSIPCCBinding::connectCandidateSignal(stream);
  }

  std::vector<std::string> candidates = stream->GetCandidates();
  CSFLogDebug( logTag, "%s: Got %lu candidates", __FUNCTION__, (unsigned long) candidates.size());

  std::string default_addr;
  int default_port;

  nsresult res = stream->GetDefaultCandidate(1, &default_addr, &default_port);
  MOZ_ASSERT(NS_SUCCEEDED(res));
  if (!NS_SUCCEEDED(res)) {
    return VCM_ERROR;
  }

  CSFLogDebug( logTag, "%s: Got default candidates %s:%d", __FUNCTION__,
    default_addr.c_str(), default_port);

  // Note: this leaks memory if we are out of memory. Oh well.
  *candidatesp = (char **) cpr_malloc(candidates.size() * sizeof(char *));
  if (!(*candidatesp))
    return VCM_ERROR;

  for (size_t i=0; i<candidates.size(); i++) {
    (*candidatesp)[i] = (char *) cpr_malloc(candidates[i].size() + 1);
    sstrncpy((*candidatesp)[i], candidates[i].c_str(), candidates[i].size() + 1);
  }
  *candidate_ctp = candidates.size();

  // Copy the default address
  *default_addrp = (char *) cpr_malloc(default_addr.size() + 1);
  if (!*default_addrp)
    return VCM_ERROR;
  sstrncpy(*default_addrp, default_addr.c_str(), default_addr.size() + 1);
  *default_portp = default_port; /* This is the signal that things are cool */
  return 0;
}


/**
 *  Gets the ICE parameters for a stream. Called "alloc" for style consistency
 *
 *  @param[in]  mcap_id  - media cap id
 *  @param[in]  group_id - group identifier to which stream belongs.
 *  @param[in]  stream_id - stream identifier
 *  @param[in]  call_handle  - call identifier
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  level - the m-line index (1-based)
 *  @param[out] default_addrp - the ICE default addr
 *  @param[out] port_allocatedp - the ICE default port
 *  @param[out] candidatesp - the ICE candidate array
 *  @param[out] candidate_ctp length of the array
 *
 *  @return 0 for success; VCM_ERROR for failure
 *
 */
short vcmRxAllocICE(cc_mcapid_t mcap_id,
                   cc_groupid_t group_id,
                   cc_streamid_t stream_id,
                   cc_call_handle_t  call_handle,
                   const char *peerconnection,
                   uint16_t level,
                   char **default_addrp, /* Out */
                   int *default_portp, /* Out */
                   char ***candidatesp, /* Out */
                   int *candidate_ctp /* Out */
                   )
{
  int ret;

  mozilla::RefPtr<NrIceCtx> ctx;
  mozilla::RefPtr<NrIceMediaStream> stream;

  // First, get a strong ref to the ICE context and stream from the
  // main thread.
  mozilla::SyncRunnable::DispatchToThread(
      VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetIceStream_m,
                        mcap_id,
                        group_id,
                        stream_id,
                        call_handle,
                        peerconnection,
                        level,
                        &ctx,
                        &stream,
                        &ret));
  if (ret)
    return ret;

  // Now get the ICE parameters from the STS thread.
  // We .forget() the strong refs so that they can be
  // released on the STS thread.
  mozilla::SyncRunnable::DispatchToThread(
      VcmSIPCCBinding::getSTSThread(),
                        WrapRunnableNMRet(&vcmRxAllocICE_s,
                        ctx.forget(),
                        stream.forget(),
                        call_handle,
                        stream_id,
                        level,
                        default_addrp,
                        default_portp,
                        candidatesp,
                        candidate_ctp,
                        &ret));

  return ret;
}

/* Get ICE global parameters (ufrag and pwd)
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[out] ufragp - where to put the ufrag
 *  @param[out] pwdp - where to put the pwd
 *
 *  @return 0 for success; VCM_ERROR for failure
 */
static short vcmGetIceParams_m(const char *peerconnection,
                               char **ufragp,
                               char **pwdp)
{
  CSFLogDebug( logTag, "%s: PC = %s", __FUNCTION__, peerconnection);

  *ufragp = *pwdp = nullptr;

 // Note: we don't acquire any media resources here, and we assume that the
  // ICE streams already exist, so we're just acquiring them. Any logic
  // to make them on demand is elsewhere.
  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  std::vector<std::string> attrs = pc.impl()->media()->
    ice_ctx()->GetGlobalAttributes();

  // Now fish through these looking for a ufrag and passwd
  char *ufrag = nullptr;
  char *pwd = nullptr;

  for (size_t i=0; i<attrs.size(); i++) {
    if (attrs[i].compare(0, strlen("ice-ufrag:"), "ice-ufrag:") == 0) {
      if (!ufrag) {
        ufrag = (char *) cpr_malloc(attrs[i].size() + 1);
        if (!ufrag)
          return VCM_ERROR;
        sstrncpy(ufrag, attrs[i].c_str(), attrs[i].size() + 1);
        ufrag[attrs[i].size()] = 0;
      }
    }

    if (attrs[i].compare(0, strlen("ice-pwd:"), "ice-pwd:") == 0) {
      pwd = (char *) cpr_malloc(attrs[i].size() + 1);
      if (!pwd)
        return VCM_ERROR;
      sstrncpy(pwd, attrs[i].c_str(), attrs[i].size() + 1);
      pwd[attrs[i].size()] = 0;
    }

  }
  if (!ufrag || !pwd) {
    MOZ_ASSERT(PR_FALSE);
    cpr_free(ufrag);
    cpr_free(pwd);
    CSFLogDebug( logTag, "%s: no ufrag or password", __FUNCTION__);
    return VCM_ERROR;
  }

  *ufragp = ufrag;
  *pwdp = pwd;

  return 0;
}

/* Get ICE global parameters (ufrag and pwd)
 *
 * This is a thunk to vcmGetIceParams_m.
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[out] ufragp - where to put the ufrag
 *  @param[out] pwdp - where to put the pwd
 *
 *  @return 0 for success; VCM_ERROR for failure
 */
short vcmGetIceParams(const char *peerconnection,
                     char **ufragp,
                     char **pwdp)
{
  int ret;
  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetIceParams_m,
                        peerconnection,
                        ufragp,
                        pwdp,
                        &ret));
  return ret;
}


/* Set remote ICE global parameters.
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  ufrag - the ufrag
 *  @param[in]  pwd - the pwd
 *  @param[in]  icelite - is peer ice lite
 *
 *  @return 0 for success; VCM_ERROR for failure
 */
static short vcmSetIceSessionParams_m(const char *peerconnection,
                                      char *ufrag,
                                      char *pwd,
                                      cc_boolean icelite)
{
  CSFLogDebug( logTag, "%s: PC = %s", __FUNCTION__, peerconnection);

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  std::vector<std::string> attributes;

  if (ufrag)
    attributes.push_back(ufrag);
  if (pwd)
    attributes.push_back(pwd);
  if (icelite) {
    attributes.push_back("ice-lite");
  }
  nsresult res = pc.impl()->media()->ice_ctx()->
    ParseGlobalAttributes(attributes);

  if (!NS_SUCCEEDED(res)) {
    CSFLogError( logTag, "%s: couldn't parse global parameters", __FUNCTION__ );
    return VCM_ERROR;
  }

  return 0;
}

/* Set remote ICE global parameters.
 *
 * This is a thunk to vcmSetIceSessionParams_m.
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  ufrag - the ufrag
 *  @param[in]  pwd - the pwd
 *  @param[in]  icelite - is peer using ice-lite
 *
 *  @return 0 success, error failure
 */
short vcmSetIceSessionParams(const char *peerconnection,
                             char *ufrag,
                             char *pwd,
                             cc_boolean icelite)
{
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmSetIceSessionParams_m,
                        peerconnection,
                        ufrag,
                        pwd,
                        icelite,
                        &ret));

  return ret;
}

/* Set ice candidate for trickle ICE.
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  icecandidate - the icecandidate
 *  @param[in]  level - the m line level
 *
 *  @return 0 success, error failure
 */
static short vcmSetIceCandidate_m(const char *peerconnection,
                                  const char *icecandidate,
                                  uint16_t level)
{
  CSFLogDebug( logTag, "%s: PC = %s", __FUNCTION__, peerconnection);

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  CSFLogDebug( logTag, "%s(): Getting stream %d", __FUNCTION__, level);
  mozilla::RefPtr<NrIceMediaStream> stream = pc.impl()->media()->
    ice_media_stream(level-1);
  if (!stream)
    return VCM_ERROR;

  nsresult rv = RUN_ON_THREAD(pc.impl()->media()->ice_ctx()->thread(),
                              WrapRunnable(stream,
                                           &NrIceMediaStream::ParseTrickleCandidate,
                                           std::string(icecandidate)),
                              NS_DISPATCH_NORMAL);

  if (!NS_SUCCEEDED(rv)) {
    CSFLogError( logTag, "%s(): Could not dispatch to ICE thread, level %u",
      __FUNCTION__, level);
    return VCM_ERROR;
  }

  // TODO(ekr@rtfm.com): generate an error if the parse
  // fails. Bug 847449.

  return 0;
}


/* Set ice candidate for trickle ICE.
 *
 * This is a thunk to vcmSetIceCandidate_m
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  icecandidate - the icecandidate
 *  @param[in]  level - the m line level
 *
 *  @return 0 success, error failure
 */
short vcmSetIceCandidate(const char *peerconnection,
                         const char *icecandidate,
                         uint16_t level)
{
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmSetIceCandidate_m,
                        peerconnection,
                        icecandidate,
                        level,
                        &ret));

  return ret;
}


/* Start ICE checks
 *  @param[in]  peerconnection - the peerconnection in use
 *  @return 0 success, error failure
 */
static short vcmStartIceChecks_m(const char *peerconnection, cc_boolean isControlling)
{
  CSFLogDebug( logTag, "%s: PC = %s", __FUNCTION__, peerconnection);

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  nsresult res;
  res = pc.impl()->media()->ice_ctx()->SetControlling(
      isControlling ? NrIceCtx::ICE_CONTROLLING : NrIceCtx::ICE_CONTROLLED);
  if (!NS_SUCCEEDED(res)) {
    CSFLogError( logTag, "%s: couldn't set controlling", __FUNCTION__ );
    return VCM_ERROR;
  }
  // TODO(ekr@rtfm.com): Figure out how to report errors here.
  // Bug 854516.
  nsresult rv = pc.impl()->media()->ice_ctx()->thread()->Dispatch(
    WrapRunnable(pc.impl()->media()->ice_ctx(), &NrIceCtx::StartChecks),
      NS_DISPATCH_NORMAL);

  if (!NS_SUCCEEDED(rv)) {
    CSFLogError( logTag, "%s(): Could not dispatch to ICE thread", __FUNCTION__);
    return VCM_ERROR;
  }

  return 0;
}

/* Start ICE checks
 *
 * This is a thunk to vcmStartIceChecks_m
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @return 0 success, error failure
 */
short vcmStartIceChecks(const char *peerconnection, cc_boolean isControlling)
{
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmStartIceChecks_m,
                        peerconnection,
                        isControlling,
                        &ret));

  return ret;
}

/* Set remote ICE media-level parameters.
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  level - the m-line
 *  @param[in]  ufrag - the ufrag
 *  @param[in]  pwd - the pwd
 *  @param[in]  candidates - the candidates
 *  @param[in]  candidate_ct - the number of candidates
 *  @return 0 success, error failure
 */
static short vcmSetIceMediaParams_m(const char *peerconnection,
                                    int level,
                                    char *ufrag,
                                    char *pwd,
                                    char **candidates,
                                    int candidate_ct)
{
  CSFLogDebug( logTag, "%s: PC = %s", __FUNCTION__, peerconnection);

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  CSFLogDebug( logTag, "%s(): Getting stream %d", __FUNCTION__, level);
  mozilla::RefPtr<NrIceMediaStream> stream = pc.impl()->media()->
    ice_media_stream(level-1);
  if (!stream)
    return VCM_ERROR;

  std::vector<std::string> attributes;

  if (ufrag)
    attributes.push_back(ufrag);
  if (pwd)
    attributes.push_back(pwd);

  for (int i = 0; i<candidate_ct; i++) {
    attributes.push_back(candidates[i]);
  }

  nsresult res = stream->ParseAttributes(attributes);

  if (!NS_SUCCEEDED(res)) {
    CSFLogError( logTag, "%s: couldn't parse global parameters", __FUNCTION__ );
    return VCM_ERROR;
  }

  return 0;
}

/* Set remote ICE media-level parameters.
 *
 * This is a thunk to vcmSetIceMediaParams_w
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  level - the m-line
 *  @param[in]  ufrag - the ufrag
 *  @param[in]  pwd - the pwd
 *  @param[in]  candidates - the candidates
 *  @param[in]   candidate_ct - the number of candidates
 *  @return 0 success, error failure
 */
short vcmSetIceMediaParams(const char *peerconnection,
                           int level,
                           char *ufrag,
                           char *pwd,
                           char **candidates,
                           int candidate_ct)
{
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmSetIceMediaParams_m,
                     peerconnection,
                     level,
                     ufrag,
                     pwd,
                     candidates,
                     candidate_ct,
                     &ret));

  return ret;
}


/*
 * Create a remote stream
 *
 *  @param[in] mcap_id - group identifier to which stream belongs.
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[out] pc_stream_id - the id of the allocated stream
 *
 *  TODO(ekr@rtfm.com): Revise along with everything else for the
 *  new stream model.
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 */
static short vcmCreateRemoteStream_m(
  cc_mcapid_t mcap_id,
  const char *peerconnection,
  int *pc_stream_id) {
  nsresult res;

  *pc_stream_id = -1;

  CSFLogDebug( logTag, "%s", __FUNCTION__);
  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  nsRefPtr<sipcc::RemoteSourceStreamInfo> info;
  res = pc.impl()->CreateRemoteSourceStreamInfo(&info);
  if (NS_FAILED(res)) {
    return VCM_ERROR;
  }

  res = pc.impl()->media()->AddRemoteStream(info, pc_stream_id);
  if (NS_FAILED(res)) {
    return VCM_ERROR;
  }

  CSFLogDebug( logTag, "%s: created remote stream with index %d",
    __FUNCTION__, *pc_stream_id);

  return 0;
}

/*
 * Create a remote stream
 *
 * This is a thunk to vcmCreateRemoteStream_m
 *
 *  @param[in] mcap_id - group identifier to which stream belongs.
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[out] pc_stream_id - the id of the allocated stream
 *
 *  TODO(ekr@rtfm.com): Revise along with everything else for the
 *  new stream model.
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 */
short vcmCreateRemoteStream(cc_mcapid_t mcap_id,
                            const char *peerconnection,
                            int *pc_stream_id)
{
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmCreateRemoteStream_m,
                        mcap_id,
                        peerconnection,
                        pc_stream_id,
                        &ret));

  return ret;
}

/*
 * Add remote stream hint
 *
 * We are sending track information up to PeerConnection before
 * the tracks exist so it knows when the stream is fully constructed.
 *
 * @param[in] peerconnection
 * @param[in] pc_stream_id
 * @param[in] is_video
 *
 * Returns: zero(0) for success; otherwise, ERROR for failure
 */
static short vcmAddRemoteStreamHint_m(
  const char *peerconnection,
  int pc_stream_id,
  cc_boolean is_video) {
  nsresult res;

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  res = pc.impl()->media()->AddRemoteStreamHint(pc_stream_id,
    is_video ? TRUE : FALSE);
  if (NS_FAILED(res)) {
    return VCM_ERROR;
  }

  CSFLogDebug( logTag, "%s: added remote stream hint %u with index %d",
    __FUNCTION__, is_video, pc_stream_id);

  return 0;
}

/*
 * Add remote stream hint
 *
 * This is a thunk to vcmAddRemoteStreamHint_m
 *
 * Returns: zero(0) for success; otherwise, ERROR for failure
 */
short vcmAddRemoteStreamHint(
  const char *peerconnection,
  int pc_stream_id,
  cc_boolean is_video) {
  short ret = 0;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmAddRemoteStreamHint_m,
                        peerconnection,
                        pc_stream_id,
                        is_video,
                        &ret));

  return ret;
}

/*
 * Get DTLS key data
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[out] digest_algp    - the digest algorithm e.g. 'sha-256'
 *  @param[in] max_digest_alg_len - available length of string
 *  @param[out] digestp        - the digest string
 *  @param[in] max_digest_len - available length of string
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 */
static short vcmGetDtlsIdentity_m(const char *peerconnection,
                                  char *digest_algp,
                                  size_t max_digest_alg_len,
                                  char *digestp,
                                  size_t max_digest_len) {

  digest_algp[0]='\0';
  digestp[0]='\0';

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  std::string algorithm = pc.impl()->GetFingerprintAlgorithm();
  sstrncpy(digest_algp, algorithm.c_str(), max_digest_alg_len);
  std::string value = pc.impl()->GetFingerprintHexValue();
  sstrncpy(digestp, value.c_str(), max_digest_len);

  return 0;
}

/*
 * Get DTLS key data
 *
 * This is a thunk to vcmGetDtlsIdentity_m
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[out] digest_algp    - the digest algorithm e.g. 'SHA-1'
 *  @param[in] max_digest_alg_len - length of string
 *  @param[out] digestp        - the digest string
 *  @param[in] max_digest_len - length of string
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 */
short vcmGetDtlsIdentity(const char *peerconnection,
                         char *digest_algp,
                         size_t max_digest_alg_len,
                         char *digestp,
                         size_t max_digest_len) {
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetDtlsIdentity_m,
                        peerconnection,
                        digest_algp,
                        max_digest_alg_len,
                        digestp,
                        max_digest_len,
                        &ret));

  return ret;
}

/* Set negotiated DataChannel parameters.
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  streams - the number of streams for this data channel
 *  @param[in]  local_datachannel_port - the negotiated sctp port
 *  @param[in]  remote_datachannel_port - the negotiated sctp port
 *  @param[in]  protocol - the protocol as a string
 *
 *  @return 0 success, error failure
 */
static short vcmInitializeDataChannel_m(const char *peerconnection,
  int track_id, cc_uint16_t streams,
  int local_datachannel_port, int remote_datachannel_port, const char* protocol)
{
  nsresult res;

  CSFLogDebug( logTag, "%s: PC = %s", __FUNCTION__, peerconnection);

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  res = pc.impl()->InitializeDataChannel(track_id, local_datachannel_port,
                                         remote_datachannel_port, streams);
  if (NS_FAILED(res)) {
    return VCM_ERROR;
  }

  return 0;
}

/* Set negotiated DataChannel parameters.
 *
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  streams - the number of streams for this data channel
 *  @param[in]  local_datachannel_port - the negotiated sctp port
 *  @param[in]  remote_datachannel_port - the negotiated sctp port
 *  @param[in]  protocol - the protocol as a string
 *
 *  @return 0 success, error failure
 */
short vcmInitializeDataChannel(const char *peerconnection, int track_id,
  cc_uint16_t streams,
  int local_datachannel_port, int remote_datachannel_port, const char* protocol)
{
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmInitializeDataChannel_m,
                        peerconnection,
                        track_id,
                        streams,
                        local_datachannel_port,
                        remote_datachannel_port,
                        protocol,
                        &ret));

  return ret;
}


/**
 *   Should we remove this from external API
 *
 *  @param[in] mcap_id - group identifier to which stream belongs.
 *  @param[in]     group_id         - group identifier
 *  @param[in]     stream_id        - stream identifier
 *  @param[in]     call_handle      - call identifier
 *  @param[in]     port_requested   - requested port.
 *  @param[in]     listen_ip        - local IP for listening
 *  @param[in]     is_multicast     - multicast stream?
 *  @param[out]    port_allocated   - allocated(reserved) port
 *
 *  tbd need to see if we can deprecate this API
 *
 *  @return       0 success, ERROR failure.
 *
 */

short vcmRxOpen(cc_mcapid_t mcap_id,
                cc_groupid_t group_id,
                cc_streamid_t stream_id,
                cc_call_handle_t call_handle,
                cc_uint16_t port_requested,
                cpr_ip_addr_t *listen_ip,
                cc_boolean is_multicast,
                int *port_allocated)
{
    char fname[] = "vcmRxOpen";

    char dottedIP[20] = "";
    *port_allocated = -1;
    if(listen_ip)
    {
        csf_sprintf(dottedIP, sizeof(dottedIP), "%u.%u.%u.%u",
                (listen_ip->u.ip4 >> 24) & 0xff, (listen_ip->u.ip4 >> 16) & 0xff,
                (listen_ip->u.ip4 >> 8) & 0xff, listen_ip->u.ip4 & 0xff );
    }

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d listen=%s:%d is_mcast=%d",
                      fname, group_id, call_handle, dottedIP, port_requested, is_multicast);

    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        CSFLogDebug( logTag, "%s: audio stream", fname);
        if ( VcmSIPCCBinding::getAudioTermination() != nullptr )
            *port_allocated = VcmSIPCCBinding::getAudioTermination()->rxOpen( group_id, stream_id,
                                                    port_requested, listen_ip ? listen_ip->u.ip4 : 0,
                                                    (is_multicast != 0) );
        break;
    case CC_VIDEO_1:
        CSFLogDebug( logTag, "%s: video stream", fname);
        if ( VcmSIPCCBinding::getVideoTermination() != nullptr )
            *port_allocated = VcmSIPCCBinding::getVideoTermination()->rxOpen( group_id, stream_id,
                                                    port_requested, listen_ip ? listen_ip->u.ip4 : 0,
                                                    (is_multicast != 0) );
        break;

    default:
        break;
    }
    return 0;
}

/**
 *  Start receive stream
 *  Note: For video calls, for a given call_handle there will be
 *        two media lines and the corresponding group_id/stream_id pair.
 *        One RTP session is requested from media server for each
 *        media line(group/stream) i.e. a video call would result in
 *        two rtp_sessions in our session info list created by two
 *        calls to vcm_rx/tx with mcap_id of AUDIO and VIDEO respectively.
 *
 *  @param[in]    mcap_id     - media type id
 *  @param[in]    group_id    - group identifier associated with the stream
 *  @param[in]    stream_id   - id of the stream one per each media line
 *  @param[in]    call_handle - call identifier
 *  @param[in]    payload     - payload information
 *  @param[in]    local_addr  - local ip address to use.
 *  @param[in]    port        - local port (receive)
 *  @param[in]    algorithmID - crypto alogrithm ID
 *  @param[in]    rx_key      - rx key used when algorithm ID is encrypting
 *  @param[in]    attrs       - media attributes
 *
 *  @return   zero(0) for success; otherwise, -1 for failure
 *
 */
int vcmRxStart(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle,
        const vcm_payload_info_t *payload,
        cpr_ip_addr_t *local_addr,
        cc_uint16_t port,
        vcm_crypto_algorithmID algorithmID,
        vcm_crypto_key_t *rx_key,
        vcm_mediaAttrs_t *attrs)
{
    uint8_t    *key;
    uint8_t    *salt;
    cc_uint16_t    key_len;
    cc_uint16_t    salt_len;
    char        fname[] = "vcmRxStart";

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d payload=%d port=%d"
        " algID=%d", fname, group_id, call_handle, payload->remote_rtp_pt,
        port, algorithmID);

    if (call_handle == CC_NO_CALL_ID) {
        /* no operation when no call ID */
        return VCM_ERROR;
    }

    /* Set up crypto parameters */
    switch ( algorithmID )
    {
    case VCM_AES_128_COUNTER:
        if ( rx_key == nullptr )
        {
            /* No key provided */
            CSFLogDebug( logTag, "vcmRxStart(): No key for algorithm ID %d",
                      algorithmID);
            return VCM_ERROR;
        }
        /* Set up key and Salt parameters */
        key_len = rx_key->key_len;
        key = &rx_key->key[0];
        salt_len = rx_key->salt_len;
        salt = &rx_key->salt[0];
        break;

    default:
        /* Give dummy data to avoid passing nullptr for key/salt */
        key_len = 0;
        key = (uint8_t *)"";
        salt_len = 0;
        salt = (uint8_t *)"";
        break;
    }

    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != nullptr )
            return VcmSIPCCBinding::getAudioTermination()->rxStart(
                group_id, stream_id, payload->local_rtp_pt,
                attrs->audio.packetization_period, port,
                attrs->audio.avt_payload_type, map_algorithmID(algorithmID),
                key, key_len, salt, salt_len, attrs->audio.mixing_mode,
                attrs->audio.mixing_party );
        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != nullptr )
            return VcmSIPCCBinding::getVideoTermination()->rxStart(
                group_id, stream_id, payload->local_rtp_pt,
                0, port, 0, map_algorithmID(algorithmID), key, key_len,
                salt, salt_len, 0, 0);
        break;

    default:
        break;
    }
    return VCM_ERROR;
}


/**
 *  start rx stream
 *  Same concept as vcmRxStart but for ICE/PeerConnection-based flows
 *
 *  @param[in]   mcap_id      - media cap id
 *  @param[in]   group_id     - group identifier to which the stream belongs
 *  @param[in]   stream_id    - stream id of the given media type.
 *  @param[in]   level        - the m-line index
 *  @param[in]   pc_stream_id - the media stream index (from PC.addStream())
 *  @param[in]   pc_track_id  - the track within the media stream
 *  @param[in]   call_handle  - call handle
 *  @param[in]   peerconnection - the peerconnection in use
 *  @param[in]   num_payloads   - number of negotiated payloads
 *  @param[in]   payloads       - negotiated codec details list
 *  @param[in]   setup          - whether playing client or server role
 *  @param[in]   fingerprint_alg - the DTLS fingerprint algorithm
 *  @param[in]   fingerprint  - the DTLS fingerprint
 *  @param[in]   attrs        - media attributes
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 */

static int vcmRxStartICE_m(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        int level,
        int pc_stream_id,
        int pc_track_id,
        cc_call_handle_t  call_handle,
        const char *peerconnection,
        int num_payloads,
        const vcm_payload_info_t* payloads,
        sdp_setup_type_e setup_type,
        const char *fingerprint_alg,
        const char *fingerprint,
        vcm_mediaAttrs_t *attrs)
{
  CSFLogDebug( logTag, "%s(%s) track = %d, stream = %d, level = %d",
              __FUNCTION__,
              peerconnection,
              pc_track_id,
              pc_stream_id,
              level);

  // Find the PC.
  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  // Datachannel will use this though not for RTP
  mozilla::RefPtr<TransportFlow> rtp_flow =
    vcmCreateTransportFlow(pc.impl(), level, false, setup_type,
                           fingerprint_alg, fingerprint);
  if (!rtp_flow) {
    CSFLogError( logTag, "Could not create RTP flow");
    return VCM_ERROR;
  }

  if (CC_IS_DATACHANNEL(mcap_id)) {
    // That's all we need for DataChannels - a flow registered
    CSFLogDebug( logTag, "%s success", __FUNCTION__);
    return 0;
  }

  if (!payloads) {
    CSFLogError( logTag, "Unitialized payload list");
    return VCM_ERROR;
  }

  // Find the stream we need
  nsRefPtr<sipcc::RemoteSourceStreamInfo> stream =
    pc.impl()->media()->GetRemoteStream(pc_stream_id);
  if (!stream) {
    // This should never happen
    PR_ASSERT(PR_FALSE);
    return VCM_ERROR;
  }

  mozilla::RefPtr<TransportFlow> rtcp_flow = nullptr;
  if (!attrs->rtcp_mux) {
    rtcp_flow = vcmCreateTransportFlow(pc.impl(), level, true, setup_type,
                                       fingerprint_alg, fingerprint);
    if (!rtcp_flow) {
      CSFLogError( logTag, "Could not create RTCP flow");
      return VCM_ERROR;
    }
  }

  // If we're offering bundle, a given MediaPipeline could receive traffic on
  // two different network flows depending on whether the answerer accepts,
  // before any answer comes in. We need to be prepared for both cases.
  nsAutoPtr<mozilla::MediaPipelineFilter> filter;
  RefPtr<TransportFlow> bundle_rtp_flow;
  RefPtr<TransportFlow> bundle_rtcp_flow;
  if (attrs->bundle_level) {
    filter = new MediaPipelineFilter;
    // Record our correlator, if present in our offer.
    filter->SetCorrelator(attrs->bundle_stream_correlator);

    // Record our own ssrcs (these are _not_ those of the remote end; that
    // is handled in vcmTxStart)
    for (int s = 0; s < attrs->num_ssrcs; ++s) {
      filter->AddLocalSSRC(attrs->ssrcs[s]);
    }

    // Record the unique payload types
    for (int p = 0; p < attrs->num_unique_payload_types; ++p) {
      filter->AddUniquePT(attrs->unique_payload_types[p]);
    }

    // Do not pass additional TransportFlows if the pipeline will use the same
    // flow regardless of whether bundle happens or not.
    if (attrs->bundle_level != (unsigned int)level) {
      // This might end up creating it, or might reuse it.
      mozilla::RefPtr<TransportFlow> bundle_rtp_flow =
        vcmCreateTransportFlow(pc.impl(),
                               attrs->bundle_level,
                               false,
                               setup_type,
                               fingerprint_alg,
                               fingerprint);

      if (!bundle_rtp_flow) {
        CSFLogError( logTag, "Could not create bundle RTP flow");
        return VCM_ERROR;
      }

      if (!attrs->rtcp_mux) {
        bundle_rtcp_flow = vcmCreateTransportFlow(pc.impl(),
                                                  attrs->bundle_level,
                                                  true,
                                                  setup_type,
                                                  fingerprint_alg,
                                                  fingerprint);
        if (!bundle_rtcp_flow) {
          CSFLogError( logTag, "Could not create bundle RTCP flow");
          return VCM_ERROR;
        }
      }
    }
  }

  if (CC_IS_AUDIO(mcap_id)) {
    std::vector<mozilla::AudioCodecConfig *> configs;

    // Instantiate an appropriate conduit
    mozilla::RefPtr<mozilla::MediaSessionConduit> tx_conduit =
      pc.impl()->media()->GetConduit(level, false);
    MOZ_ASSERT_IF(tx_conduit, tx_conduit->type() == MediaSessionConduit::AUDIO);

    // The two sides of a send/receive pair of conduits each keep a raw pointer to the other,
    // and are responsible for cleanly shutting down.
    mozilla::RefPtr<mozilla::AudioSessionConduit> conduit =
      mozilla::AudioSessionConduit::Create(static_cast<AudioSessionConduit *>(tx_conduit.get()));
    if(!conduit)
      return VCM_ERROR;

    pc.impl()->media()->AddConduit(level, true, conduit);

    mozilla::AudioCodecConfig *config_raw;

    for(int i=0; i <num_payloads ; i++)
    {
      config_raw = new mozilla::AudioCodecConfig(
        payloads[i].local_rtp_pt,
        ccsdpCodecName(payloads[i].codec_type),
        payloads[i].audio.frequency,
        payloads[i].audio.packet_size,
        payloads[i].audio.channels,
        payloads[i].audio.bitrate);
      configs.push_back(config_raw);
    }

    if (conduit->ConfigureRecvMediaCodecs(configs))
      return VCM_ERROR;

    // Now we have all the pieces, create the pipeline
    mozilla::RefPtr<mozilla::MediaPipelineReceiveAudio> pipeline =
      new mozilla::MediaPipelineReceiveAudio(
        pc.impl()->GetHandle(),
        pc.impl()->GetMainThread().get(),
        pc.impl()->GetSTSThread(),
        stream->GetMediaStream()->GetStream(),
        pc_track_id,
        level,
        conduit,
        rtp_flow,
        rtcp_flow,
        bundle_rtp_flow,
        bundle_rtcp_flow,
        filter);

    nsresult res = pipeline->Init();
    if (NS_FAILED(res)) {
      CSFLogError(logTag, "Failure initializing audio pipeline");
      return VCM_ERROR;
    }

    CSFLogDebug(logTag, "Created audio pipeline %p, conduit=%p, pc_stream=%d pc_track=%d",
                pipeline.get(), conduit.get(), pc_stream_id, pc_track_id);

    stream->StorePipeline(pc_track_id, false, pipeline);
  } else if (CC_IS_VIDEO(mcap_id)) {

    std::vector<mozilla::VideoCodecConfig *> configs;
    // Instantiate an appropriate conduit
    mozilla::RefPtr<mozilla::MediaSessionConduit> tx_conduit =
      pc.impl()->media()->GetConduit(level, false);
    MOZ_ASSERT_IF(tx_conduit, tx_conduit->type() == MediaSessionConduit::VIDEO);

    // The two sides of a send/receive pair of conduits each keep a raw pointer to the other,
    // and are responsible for cleanly shutting down.
    mozilla::RefPtr<mozilla::VideoSessionConduit> conduit =
      mozilla::VideoSessionConduit::Create(static_cast<VideoSessionConduit *>(tx_conduit.get()));
    if(!conduit)
      return VCM_ERROR;

    pc.impl()->media()->AddConduit(level, true, conduit);

    mozilla::VideoCodecConfig *config_raw;

    for(int i=0; i <num_payloads; i++)
    {
      config_raw = new mozilla::VideoCodecConfig(
        payloads[i].local_rtp_pt,
        ccsdpCodecName(payloads[i].codec_type),
        payloads[i].video.rtcp_fb_types);
      if (vcmEnsureExternalCodec(conduit, config_raw, false)) {
        continue;
      }
      configs.push_back(config_raw);
    }

    if (conduit->ConfigureRecvMediaCodecs(configs))
      return VCM_ERROR;

    // Now we have all the pieces, create the pipeline
    mozilla::RefPtr<mozilla::MediaPipelineReceiveVideo> pipeline =
        new mozilla::MediaPipelineReceiveVideo(
            pc.impl()->GetHandle(),
            pc.impl()->GetMainThread().get(),
            pc.impl()->GetSTSThread(),
            stream->GetMediaStream()->GetStream(),
            pc_track_id,
            level,
            conduit,
            rtp_flow,
            rtcp_flow,
            bundle_rtp_flow,
            bundle_rtcp_flow,
            filter);

    nsresult res = pipeline->Init();
    if (NS_FAILED(res)) {
      CSFLogError(logTag, "Failure initializing video pipeline");
      return VCM_ERROR;
    }

    CSFLogDebug(logTag, "Created video pipeline %p, conduit=%p, pc_stream=%d pc_track=%d",
                pipeline.get(), conduit.get(), pc_stream_id, pc_track_id);

    stream->StorePipeline(pc_track_id, true, pipeline);
  } else {
    CSFLogError(logTag, "%s: mcap_id unrecognized", __FUNCTION__);
    return VCM_ERROR;
  }

  CSFLogDebug( logTag, "%s success", __FUNCTION__);
  return 0;
}


/**
 *  start rx stream
 *  Same concept as vcmRxStart but for ICE/PeerConnection-based flows
 *
 *  This is a thunk to vcmRxStartICE_m
 *
 *  @param[in]   mcap_id      - media cap id
 *  @param[in]   group_id     - group identifier to which the stream belongs
 *  @param[in]   stream_id    - stream id of the given media type.
 *  @param[in]   level        - the m-line index
 *  @param[in]   pc_stream_id - the media stream index (from PC.addStream())
 *  @param[i]n   pc_track_id  - the track within the media stream
 *  @param[in]   call_handle  - call handle
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]  num_payloads   - number of negotiated payloads
 *  @param[in]  payloads       - negotiated codec details list
 *  @param[in]   setup_type    - whether playing client or server role
 *  @param[in]   fingerprint_alg - the DTLS fingerprint algorithm
 *  @param[in]   fingerprint  - the DTLS fingerprint
 *  @param[in]   attrs        - media attributes
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 */

int vcmRxStartICE(cc_mcapid_t mcap_id,
                  cc_groupid_t group_id,
                  cc_streamid_t stream_id,
                  int level,
                  int pc_stream_id,
                  int pc_track_id,
                  cc_call_handle_t  call_handle,
                  const char *peerconnection,
                  int num_payloads,
                  const vcm_payload_info_t* payloads,
                  sdp_setup_type_e setup_type,
                  const char *fingerprint_alg,
                  const char *fingerprint,
                  vcm_mediaAttrs_t *attrs)
{
  int ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmRxStartICE_m,
                        mcap_id,
                        group_id,
                        stream_id,
                        level,
                        pc_stream_id,
                        pc_track_id,
                        call_handle,
                        peerconnection,
                        num_payloads,
                        payloads,
                        setup_type,
                        fingerprint_alg,
                        fingerprint,
                        attrs,
                        &ret));

  return ret;
}


/**
 *  Close the receive stream.
 *
 *  @param[in]    mcap_id - Media Capability ID
 *  @param[in]    group_id - group identifier that belongs to the stream.
 *  @param[in]    stream_id - stream id of the given media type.
 *  @param[in]    call_handle  - call identifier
 *
 *  @return 0 for success; VCM_ERROR for failure
 *
 */

short vcmRxClose(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle)
{
    char fname[] = "vcmRxClose";

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d", fname, group_id, call_handle);

    if (call_handle == CC_NO_CALL_ID) {
        CSFLogDebug( logTag, "No CALL ID");
        /* no operation when no call ID */
        return VCM_ERROR;
    }
    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != nullptr )
            VcmSIPCCBinding::getAudioTermination()->rxClose( group_id, stream_id );
        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != nullptr )
            VcmSIPCCBinding::getVideoTermination()->rxClose( group_id, stream_id );
        break;

    default:
        break;
    }
    return 0;
}

/**
 *  Release the allocated port
 * @param[in] mcap_id   - media capability id (0 is audio)
 * @param[in] group_id  - group identifier
 * @param[in] stream_id - stream identifier
 * @param[in] call_handle   - call id
 * @param[in] port     - port to be released
 *
 * @return void
 */

void vcmRxReleasePort  (cc_mcapid_t mcap_id,
                        cc_groupid_t group_id,
                        cc_streamid_t stream_id,
                        cc_call_handle_t  call_handle,
                        int port)
{
    CSFLogDebug( logTag, "vcmRxReleasePort(): group_id=%d stream_id=%d call_handle=%d port=%d",
                      group_id, stream_id, call_handle, port);

    if(CC_IS_AUDIO(mcap_id))
    {
        if ( VcmSIPCCBinding::getAudioTermination() != nullptr )
            VcmSIPCCBinding::getAudioTermination()->rxRelease( group_id, stream_id, port );
    }
    else if(CC_IS_VIDEO(mcap_id))
    {
        if ( VcmSIPCCBinding::getVideoTermination() != nullptr )
           VcmSIPCCBinding::getVideoTermination()->rxRelease( group_id, stream_id, port );
    }

    StreamObserver* obs = VcmSIPCCBinding::getStreamObserver();
    if(obs != nullptr)
        obs->deregisterStream(call_handle, stream_id);
}

/*
 *  Function:map_tone_type
 *
 *  Description: Convert to corresponding JPhone tone.
 *
 *  Parameters:  tone - vcm tone
 *
 *  Returns:     Mapped tone type
 *
 */
static ToneType
map_tone_type( vcm_tones_t tone )
{
    switch ( tone )
    {
    case VCM_INSIDE_DIAL_TONE:
        return ToneType_INSIDE_DIAL_TONE;
    case VCM_OUTSIDE_DIAL_TONE:
        return ToneType_OUTSIDE_DIAL_TONE;
    case VCM_LINE_BUSY_TONE:
        return ToneType_LINE_BUSY_TONE;
    case VCM_ALERTING_TONE:
        return ToneType_ALERTING_TONE;
    case VCM_BUSY_VERIFY_TONE:
        return ToneType_BUSY_VERIFY_TONE;
    case VCM_STUTTER_TONE:
        return ToneType_STUTTER_TONE;
    case VCM_MSG_WAITING_TONE:
        return ToneType_MSG_WAITING_TONE;
    case VCM_REORDER_TONE:
        return ToneType_REORDER_TONE;
    case VCM_CALL_WAITING_TONE:
        return ToneType_CALL_WAITING_TONE;
    case VCM_CALL_WAITING_2_TONE:
        return ToneType_CALL_WAITING_2_TONE;
    case VCM_CALL_WAITING_3_TONE:
        return ToneType_CALL_WAITING_3_TONE;
    case VCM_CALL_WAITING_4_TONE:
        return ToneType_CALL_WAITING_4_TONE;
    case VCM_HOLD_TONE:
        return ToneType_HOLD_TONE;
    case VCM_CONFIRMATION_TONE:
        return ToneType_CONFIRMATION_TONE;
    case VCM_PERMANENT_SIGNAL_TONE:
        return ToneType_PERMANENT_SIGNAL_TONE;
    case VCM_REMINDER_RING_TONE:
        return ToneType_REMINDER_RING_TONE;
    case VCM_NO_TONE:
        return ToneType_NO_TONE;
    case VCM_ZIP_ZIP:
        return ToneType_ZIP_ZIP;
    case VCM_ZIP:
        return ToneType_ZIP;
    case VCM_BEEP_BONK:
        return ToneType_BEEP_BONK;
    case VCM_RECORDERWARNING_TONE:
        return ToneType_RECORDERWARNING_TONE;
    case VCM_RECORDERDETECTED_TONE:
        return ToneType_RECORDERDETECTED_TONE;
    case VCM_MONITORWARNING_TONE:
        return ToneType_MONITORWARNING_TONE;
    case VCM_SECUREWARNING_TONE:
        return ToneType_SECUREWARNING_TONE;
    default:
        CSFLogDebug( logTag, "map_tone_type(): WARNING..tone type not mapped.");
        return ToneType_NO_TONE;
    }
}

/**
 *  Start a tone (continuous)
 *
 *  Parameters:
 *  @param[in] tone       - tone type
 *  @param[in] alert_info - alertinfo header
 *  @param[in] call_handle    - call identifier
 *  @param[in] group_id - identifier of the group to which stream belongs
 *  @param[in] stream_id   - stream identifier.
 *  @param[in] direction  - network, speaker, both
 *
 *  @return void
 *
 */

void vcmToneStart(vcm_tones_t tone,
        short alert_info,
        cc_call_handle_t  call_handle,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_uint16_t direction)
{
    const char fname[] = "vcmToneStart";

    CSFLogDebug( logTag, "%s:tone=%d call_handle=%d dir=%d", fname, tone, call_handle, direction);

    VcmSIPCCBinding::getAudioTermination()->toneStart( map_tone_type(tone), (ToneDirection)direction,
                                alert_info, group_id, stream_id, FALSE );
    /*
     * Set delay value for multi-part tones and repeated tones.
     * Currently the only multi-part tones are stutter and message
     * waiting tones. The only repeated tones are call waiting and
     * tone on hold tones.  If the DSP ever supports stutter and
     * message waiting tones, these tones can be removed from this
     * switch statement.
     */

    /*
    switch ( tone )
    {
    case VCM_MSG_WAITING_TONE:
        lsm_start_multipart_tone_timer(tone, MSG_WAITING_DELAY, call_handle);
        break;

    case VCM_HOLD_TONE:
        lsm_start_continuous_tone_timer(tone, TOH_DELAY, call_handle);
        break;

    default:
        break;
    }
    */

    /*
     * Update dcb->active_tone if start request
     * is for an infinite duration tone.
     */
    //lsm_update_active_tone(tone, call_handle);
}

/**
 * Plays a short tone. uses the open audio path.
 * If no audio path is open, plays on speaker.
 *
 * @param[in] tone       - tone type
 * @param[in] alert_info - alertinfo header
 * @param[in] call_handle    - call identifier
 * @param[in] group_id - identifier of the group to which stream belongs
 * @param[in] stream_id   - stream identifier.
 * @param[in] direction  - network, speaker, both
 *
 * @return void
 */

void vcmToneStartWithSpeakerAsBackup(vcm_tones_t tone,
        short alert_info,
        cc_call_handle_t  call_handle,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_uint16_t direction)
{
    const char fname[] = "vcmToneStartWithSpeakerAsBackup";

    CSFLogDebug( logTag, "%s:tone=%d call_handle=%d dir=%d", fname, tone, call_handle, direction);

    VcmSIPCCBinding::getAudioTermination()->toneStart( map_tone_type(tone), (ToneDirection)direction,
                                alert_info, group_id, stream_id, TRUE );
    /*
     * Update dcb->active_tone if start request
     * is for an infinite duration tone.
     */
    //lsm_update_active_tone(tone, call_handle);
}

/**
 *  Stop the tone being played.
 *
 *  Description: Stop the tone being played currently
 *
 *
 * @param[in] tone - tone to be stopeed
 * @param[in] group_id - associated stream's group
 * @param[in] stream_id - associated stream id
 * @param[in] call_handle - the context (call) for this tone.
 *
 * @return void
 *
 */

void vcmToneStop(vcm_tones_t tone,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t call_handle)
{
    const char fname[] = "vcmToneStop";

    CSFLogDebug( logTag, "%s:tone=%d stream_id=%d", fname, tone, stream_id);
/*
    lsm_stop_multipart_tone_timer();
    lsm_stop_continuous_tone_timer();
    */

    VcmSIPCCBinding::getAudioTermination()->toneStop( map_tone_type(tone), group_id, stream_id );
}

/**
 *  should we remove from external API
 *
 *  @param[in]  mcap_id - Media Capability ID
 *  @param[in]  group_id - group to which stream belongs
 *  @param[in]  stream_id - stream identifier
 *  @param[in]  call_handle - call identifier
 *
 *  @return  zero(0) for success; otherwise, ERROR for failure
 *
 */

short vcmTxOpen(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle)
{
    char        fname[] = "vcmTxOpen";

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d", fname, group_id, call_handle);

    if (call_handle == CC_NO_CALL_ID) {
        /* no operation when no call ID */
        return VCM_ERROR;
    }
    return 0;
}

/*
 * Add external H.264 video codec.
 */
static int vcmEnsureExternalCodec(
    const mozilla::RefPtr<mozilla::VideoSessionConduit>& conduit,
    mozilla::VideoCodecConfig* config,
    bool send)
{
  if (config->mName == "VP8") {
    // whitelist internal codecs; I420 will be here once we resolve bug 995884
    return 0;
  } else if (config->mName == "H264_P0" || config->mName == "H264_P1") {
    // Here we use "I420" to register H.264 because WebRTC.org code has a
    // whitelist of supported video codec in |webrtc::ViECodecImpl::CodecValid()|
    // and will reject registration of those not in it.
    // TODO: bug 995884 to support H.264 in WebRTC.org code.

    // Register H.264 codec.
    if (send) {
	VideoEncoder* encoder = nullptr;
#ifdef MOZ_WEBRTC_OMX
	encoder = OMXVideoCodec::CreateEncoder(
	    OMXVideoCodec::CodecType::CODEC_H264);
#else
	encoder = mozilla::GmpVideoCodec::CreateEncoder();
#endif
      if (encoder) {
        return conduit->SetExternalSendCodec(config, encoder);
      } else {
        return kMediaConduitInvalidSendCodec;
      }
    } else {
      VideoDecoder* decoder;
#ifdef MOZ_WEBRTC_OMX
      decoder = OMXVideoCodec::CreateDecoder(OMXVideoCodec::CodecType::CODEC_H264);
#else
      decoder = mozilla::GmpVideoCodec::CreateDecoder();
#endif
      if (decoder) {
        return conduit->SetExternalRecvCodec(config, decoder);
      } else {
        return kMediaConduitInvalidReceiveCodec;
      }
    }
    NS_NOTREACHED("Shouldn't get here!");
  } else {
    CSFLogError( logTag, "%s: Invalid video codec configured: %s", __FUNCTION__, config->mName.c_str());
    return send ? kMediaConduitInvalidSendCodec : kMediaConduitInvalidReceiveCodec;
  }

  return 0;
}

/**
 *  start tx stream
 *  Note: For video calls, for a given call_handle there will be
 *        two media lines and the corresponding group_id/stream_id pair.
 *        One RTP session is requested from media server for each
 *        media line(group/stream) i.e. a video call would result in
 *        two rtp_sessions in our session info list created by two
 *        calls to vcm_rx/tx with mcap_id of AUDIO and VIDEO respectively.
 *
 *  @param[in]   mcap_id      - media cap id
 *  @param[in]   group_id     - group identifier to which the stream belongs
 *  @param[in]   stream_id    - stream id of the given media type.
 *  @param[in]   call_handle      - call identifier
 *  @param[in]   payload      - payload information
 *  @param[in]   tos          - bit marking
 *  @param[in]   local_addr   - local address
 *  @param[in]   local_port   - local port
 *  @param[in]   remote_ip_addr - remote ip address
 *  @param[in]   remote_port  - remote port
 *  @param[in]   algorithmID  - crypto alogrithm ID
 *  @param[in]   tx_key       - tx key used when algorithm ID is encrypting.
 *  @param[in]   attrs        - media attributes
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 *
 */
int vcmTxStart(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle,
        const vcm_payload_info_t *payload,
        short tos,
        cpr_ip_addr_t *local_addr,
        cc_uint16_t local_port,
        cpr_ip_addr_t *remote_ip_addr,
        cc_uint16_t remote_port,
        vcm_crypto_algorithmID algorithmID,
        vcm_crypto_key_t *tx_key,
        vcm_mediaAttrs_t *attrs)
{
    const char fname[] = "vcmTxStart";
    uint8_t    *key;
    uint8_t    *salt;
    cc_uint16_t    key_len;
    cc_uint16_t    salt_len;

    char dottedIP[20];
    csf_sprintf(dottedIP, sizeof(dottedIP), "%u.%u.%u.%u",
                (remote_ip_addr->u.ip4 >> 24) & 0xff, (remote_ip_addr->u.ip4 >> 16) & 0xff,
                (remote_ip_addr->u.ip4 >> 8) & 0xff, remote_ip_addr->u.ip4 & 0xff );

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d payload=%d tos=%d local_port=%d remote=%s:%d algID=%d",
        fname, group_id, call_handle, payload->remote_rtp_pt, tos, local_port,
        dottedIP, remote_port, algorithmID);

    if (call_handle == CC_NO_CALL_ID) {
        /* no operation when no call ID */
        return VCM_ERROR;
    }

    /* Set up crypto parameters */
    switch ( algorithmID )
    {
    case VCM_AES_128_COUNTER:
        if ( tx_key == nullptr )
        {
            /* No key provided */
            CSFLogDebug( logTag, "%s: No key for algorithm ID %d", fname, algorithmID);
            return VCM_ERROR;
        }
        /* Set up key and Salt parameters */
        key_len  = tx_key->key_len;
        key      = &tx_key->key[0];
        salt_len = tx_key->salt_len;
        salt     = &tx_key->salt[0];
        break;

    default:
        /* Give dummy data to avoid passing nullptr for key/salt */
        key_len  = 0;
        key      = (uint8_t *)"";
        salt_len = 0;
        salt     = (uint8_t *)"";
        break;
    }

    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != nullptr )
            return VcmSIPCCBinding::getAudioTermination()->txStart(
                group_id, stream_id, payload->remote_rtp_pt,
                attrs->audio.packetization_period, (attrs->audio.vad != 0),
                tos, dottedIP, remote_port, attrs->audio.avt_payload_type,
                map_algorithmID(algorithmID), key, key_len, salt, salt_len,
                attrs->audio.mixing_mode, attrs->audio.mixing_party );

        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != nullptr )
           return VcmSIPCCBinding::getVideoTermination()->txStart(
              group_id, stream_id, payload->remote_rtp_pt,
              0, 0, tos, dottedIP, remote_port, 0,
              map_algorithmID(algorithmID), key, key_len, salt, salt_len, 0, 0);
        break;

    default:
        break;
    }
    return VCM_ERROR;
}

/**
 * Create a conduit for audio transmission.
 *
 *  @param[in]   level        - the m-line index
 *  @param[in]   payload      - codec info
 *  @param[in]   pc           - the peer connection
 *  @param [in]  attrs        - additional audio attributes
 *  @param[out]  conduit      - the conduit to create
 */
static int vcmTxCreateAudioConduit(int level,
                                   const vcm_payload_info_t *payload,
                                   sipcc::PeerConnectionWrapper &pc,
                                   const vcm_mediaAttrs_t *attrs,
                                   mozilla::RefPtr<mozilla::MediaSessionConduit> &conduit)
{
  mozilla::AudioCodecConfig *config_raw =
    new mozilla::AudioCodecConfig(
      payload->remote_rtp_pt,
      ccsdpCodecName(payload->codec_type),
      payload->audio.frequency,
      payload->audio.packet_size,
      payload->audio.channels,
      payload->audio.bitrate);

  // Take possession of this pointer
  mozilla::ScopedDeletePtr<mozilla::AudioCodecConfig> config(config_raw);

  // Instantiate an appropriate conduit
  mozilla::RefPtr<mozilla::MediaSessionConduit> rx_conduit =
    pc.impl()->media()->GetConduit(level, true);
  MOZ_ASSERT_IF(rx_conduit, rx_conduit->type() == MediaSessionConduit::AUDIO);

  // The two sides of a send/receive pair of conduits each keep a raw pointer to the other,
  // and are responsible for cleanly shutting down.
  mozilla::RefPtr<mozilla::AudioSessionConduit> tx_conduit =
    mozilla::AudioSessionConduit::Create(
      static_cast<AudioSessionConduit *>(rx_conduit.get()));

  if (!tx_conduit || tx_conduit->ConfigureSendMediaCodec(config) ||
      tx_conduit->EnableAudioLevelExtension(attrs->audio_level,
                                            attrs->audio_level_id)) {
    return VCM_ERROR;
  }
  CSFLogError(logTag, "Created audio pipeline audio level %d %d",
              attrs->audio_level, attrs->audio_level_id);

  conduit = tx_conduit;
  return 0;
}

/**
 * Create a conduit for video transmission.
 *
 *  @param[in]   level        - the m-line index
 *  @param[in]   payload      - codec info
 *  @param[in]   pc           - the peer connection
 *  @param[out]  conduit      - the conduit to create
 */
static int vcmTxCreateVideoConduit(int level,
                                   const vcm_payload_info_t *payload,
                                   sipcc::PeerConnectionWrapper &pc,
                                   const vcm_mediaAttrs_t *attrs,
                                   mozilla::RefPtr<mozilla::MediaSessionConduit> &conduit)
{
  mozilla::VideoCodecConfig *config_raw;
  struct VideoCodecConfigH264 *negotiated = nullptr;

  if (attrs->video.opaque &&
      (payload->codec_type == RTP_H264_P0 || payload->codec_type == RTP_H264_P1)) {
    negotiated = static_cast<struct VideoCodecConfigH264 *>(attrs->video.opaque);
  }

  config_raw = new mozilla::VideoCodecConfig(
    payload->remote_rtp_pt,
    ccsdpCodecName(payload->codec_type),
    payload->video.rtcp_fb_types,
    payload->video.max_fs,
    payload->video.max_fr,
    negotiated);

  // Take possession of this pointer
  mozilla::ScopedDeletePtr<mozilla::VideoCodecConfig> config(config_raw);

  // Instantiate an appropriate conduit
  mozilla::RefPtr<mozilla::MediaSessionConduit> rx_conduit =
    pc.impl()->media()->GetConduit(level, true);
  MOZ_ASSERT_IF(rx_conduit, rx_conduit->type() == MediaSessionConduit::VIDEO);

  // The two sides of a send/receive pair of conduits each keep a raw pointer to the other,
  // and are responsible for cleanly shutting down.
  mozilla::RefPtr<mozilla::VideoSessionConduit> tx_conduit =
    mozilla::VideoSessionConduit::Create(static_cast<VideoSessionConduit *>(rx_conduit.get()));
  if (!tx_conduit) {
    return VCM_ERROR;
  }
  if (vcmEnsureExternalCodec(tx_conduit, config, true)) {
    return VCM_ERROR;
  }
  if (tx_conduit->ConfigureSendMediaCodec(config)) {
    return VCM_ERROR;
  }
  conduit = tx_conduit;
  return 0;
}

/**
 *  start tx stream
 *  Same concept as vcmTxStart but for ICE/PeerConnection-based flows
 *
 *  @param[in]   mcap_id      - media cap id
 *  @param[in]   group_id     - group identifier to which the stream belongs
 *  @param[in]   stream_id    - stream id of the given media type.
 *  @param[in]   level        - the m-line index
 *  @param[in]   pc_stream_id - the media stream index (from PC.addStream())
 *  @param[in]   pc_track_id  - the track within the media stream
 *  @param[in]   call_handle  - call handle
 *  @param[in]   peerconnection - the peerconnection in use
 *  @param[in]   payload      - payload information
 *  @param[in]   tos          - bit marking
 *  @param[in]   setup_type   - whether playing the client or server role
 *  @param[in]   fingerprint_alg - the DTLS fingerprint algorithm
 *  @param[in]   fingerprint  - the DTLS fingerprint
 *  @param[in]   attrs        - media attributes
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 *
 */
#define EXTRACT_DYNAMIC_PAYLOAD_TYPE(PTYPE) ((PTYPE)>>16)

static int vcmTxStartICE_m(cc_mcapid_t mcap_id,
                           cc_groupid_t group_id,
                           cc_streamid_t stream_id,
                           int level,
                           int pc_stream_id,
                           int pc_track_id,
                           cc_call_handle_t  call_handle,
                           const char *peerconnection,
                           const vcm_payload_info_t *payload,
                           short tos,
                           sdp_setup_type_e setup_type,
                           const char *fingerprint_alg,
                           const char *fingerprint,
                           vcm_mediaAttrs_t *attrs)
{
  CSFLogDebug(logTag, "%s(%s) track = %d, stream = %d, level = %d",
              __FUNCTION__,
              peerconnection,
              pc_track_id,
              pc_stream_id,
              level);

  // Find the PC and get the stream
  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);
  nsRefPtr<sipcc::LocalSourceStreamInfo> stream =
    pc.impl()->media()->GetLocalStream(pc_stream_id);
  if (!stream) {
    CSFLogError(logTag, "Stream not found");
    return VCM_ERROR;
  }

  // Create the transport flows
  mozilla::RefPtr<TransportFlow> rtp_flow =
    vcmCreateTransportFlow(pc.impl(), level, false, setup_type,
                           fingerprint_alg, fingerprint);
  if (!rtp_flow) {
    CSFLogError(logTag, "Could not create RTP flow");
    return VCM_ERROR;
  }

  mozilla::RefPtr<TransportFlow> rtcp_flow = nullptr;
  if (!attrs->rtcp_mux) {
    rtcp_flow = vcmCreateTransportFlow(pc.impl(), level, true, setup_type,
                                       fingerprint_alg, fingerprint);
    if (!rtcp_flow) {
      CSFLogError( logTag, "Could not create RTCP flow");
      return VCM_ERROR;
    }
  }

  const char *mediaType;
  mozilla::RefPtr<mozilla::MediaSessionConduit> conduit;
  int err = VCM_ERROR;
  if (CC_IS_AUDIO(mcap_id)) {
    mediaType = "audio";
    err = vcmTxCreateAudioConduit(level, payload, pc, attrs, conduit);
  } else if (CC_IS_VIDEO(mcap_id)) {
    mediaType = "video";
    err = vcmTxCreateVideoConduit(level, payload, pc, attrs, conduit);
  } else {
    CSFLogError(logTag, "%s: mcap_id unrecognized", __FUNCTION__);
  }
  if (err) {
    return err;
  }

  pc.impl()->media()->AddConduit(level, false, conduit);

  // Now we have all the pieces, create the pipeline
  mozilla::RefPtr<mozilla::MediaPipelineTransmit> pipeline =
    new mozilla::MediaPipelineTransmit(
      pc.impl()->GetHandle(),
      pc.impl()->GetMainThread().get(),
      pc.impl()->GetSTSThread(),
      stream->GetMediaStream(),
      pc_track_id,
      level,
      conduit, rtp_flow, rtcp_flow);

  nsresult res = pipeline->Init();
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "Failure initializing %s pipeline", mediaType);
    return VCM_ERROR;
  }
#ifdef MOZILLA_INTERNAL_API
  // implement checking for peerIdentity (where failure == black/silence)
  nsIDocument* doc = pc.impl()->GetWindow()->GetExtantDoc();
  if (doc) {
    pipeline->UpdateSinkIdentity_m(doc->NodePrincipal(), pc.impl()->GetPeerIdentity());
  } else {
    CSFLogError(logTag, "Initializing pipeline without attached doc");
  }
#endif

  CSFLogDebug(logTag,
              "Created %s pipeline %p, conduit=%p, pc_stream=%d pc_track=%d",
              mediaType, pipeline.get(), conduit.get(),
              pc_stream_id, pc_track_id);
  stream->StorePipeline(pc_track_id, pipeline);

  // This tells the receive MediaPipeline (if there is one) whether we are
  // doing bundle, and if so, updates the filter. Once the filter is finalized,
  // it is then copied to the transmit pipeline so it can filter RTCP.
  if (attrs->bundle_level) {
    nsAutoPtr<mozilla::MediaPipelineFilter> filter (new MediaPipelineFilter);
    for (int s = 0; s < attrs->num_ssrcs; ++s) {
      filter->AddRemoteSSRC(attrs->ssrcs[s]);
    }
    pc.impl()->media()->SetUsingBundle_m(level, true);
    pc.impl()->media()->UpdateFilterFromRemoteDescription_m(level, filter);
  } else {
    // This will also clear the filter.
    pc.impl()->media()->SetUsingBundle_m(level, false);
  }

  CSFLogDebug( logTag, "%s success", __FUNCTION__);
  return 0;
}

/**
 *  start tx stream
 *  Same concept as vcmTxStart but for ICE/PeerConnection-based flows
 *
 *  This is a thunk to vcmTxStartICE_m
 *
 *  @param[in]   mcap_id      - media cap id
 *  @param[in]   group_id     - group identifier to which the stream belongs
 *  @param[in]   stream_id    - stream id of the given media type.
 *  @param[in]   level        - the m-line index
 *  @param[in]   pc_stream_id - the media stream index (from PC.addStream())
 *  @param[i]n   pc_track_id  - the track within the media stream
 *  @param[in]   call_handle  - call handle
 *  @param[in]  peerconnection - the peerconnection in use
 *  @param[in]   payload      - payload type
 *  @param[in]   tos          - bit marking
 *  @param[in]   setup_type   - whether playing client or server role.
 *  @param[in]   fingerprint_alg - the DTLS fingerprint algorithm
 *  @param[in]   fingerprint  - the DTLS fingerprint
 *  @param[in]   attrs        - media attributes
 *
 *  Returns: zero(0) for success; otherwise, ERROR for failure
 *
 */
#define EXTRACT_DYNAMIC_PAYLOAD_TYPE(PTYPE) ((PTYPE)>>16)

int vcmTxStartICE(cc_mcapid_t mcap_id,
                  cc_groupid_t group_id,
                  cc_streamid_t stream_id,
                  int level,
                  int pc_stream_id,
                  int pc_track_id,
                  cc_call_handle_t  call_handle,
                  const char *peerconnection,
                  const vcm_payload_info_t *payload,
                  short tos,
                  sdp_setup_type_e setup_type,
                  const char *fingerprint_alg,
                  const char *fingerprint,
                  vcm_mediaAttrs_t *attrs)
{
  int ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmTxStartICE_m,
                        mcap_id,
                        group_id,
                        stream_id,
                        level,
                        pc_stream_id,
                        pc_track_id,
                        call_handle,
                        peerconnection,
                        payload,
                        tos,
                        setup_type,
                        fingerprint_alg,
                        fingerprint,
                        attrs,
                        &ret));

  return ret;
}


/**
 *  Close the transmit stream
 *
 *  @param[in] mcap_id - Media Capability ID
 *  @param[in] group_id - identifier of the group to which stream belongs
 *  @param[in] stream_id - stream id of the given media type.
 *  @param[in] call_handle  - call identifier
 *
 *  @return 0 for success; VCM_ERROR for failure
 */

short vcmTxClose(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle)
{
    const char fname[] = "vcmTxClose";

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d", fname, group_id, call_handle);

    if (call_handle == CC_NO_CALL_ID) {
        /* no operation when no call ID */
        return VCM_ERROR;
    }

    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != nullptr )
            VcmSIPCCBinding::getAudioTermination()->txClose( group_id, stream_id);
        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != nullptr )
           VcmSIPCCBinding::getVideoTermination()->txClose( group_id, stream_id);
        break;

    default:
        break;
    }
    return 0;
}

#if 0
static CodecRequestType
map_codec_request_type( int request_type )
{
    switch ( request_type )
    {
    default:
    case cip_sipcc_CodecMask_DSP_IGNORE:     return CodecRequestType_IGNORE;
    case cip_sipcc_CodecMask_DSP_DECODEONLY: return CodecRequestType_DECODEONLY;
    case cip_sipcc_CodecMask_DSP_ENCODEONLY: return CodecRequestType_ENCODEONLY;
    case cip_sipcc_CodecMask_DSP_FULLDUPLEX: return CodecRequestType_FULLDUPLEX;
    }
}
#endif

/**
 * Get current list of audio codec that could be used
 * @param request_type -
 * The request_type should be VCM_DECODEONLY/ENCODEONLY/FULLDUPLEX/IGNORE
 *
 * @return A bit mask should be returned that specifies the list of the audio
 * codecs. The bit mask should conform to the defines in this file.
 * #define VCM_RESOURCE_G711     0x00000001
 * #define VCM_RESOURCE_G729A    0x00000002
 * ....
 */

int vcmGetAudioCodecList(int request_type)
{
// Direct access to media layer replaced by locally stored codecMask
// set in PeerConnectionImpl
#if 0
    const char fname[] = "vcmGetAudioCodecList";
    int retVal;
    int codecMask = 0;

    CSFLogDebug( logTag, "%s(request_type = %d)", fname, request_type);

    retVal = VcmSIPCCBinding::getAudioTermination() ? VcmSIPCCBinding::getAudioTermination()->getCodecList( map_codec_request_type(request_type) ) : 0;

    if ( retVal & AudioCodecMask_G711 ) {    codecMask |= cip_sipcc_CodecMask_DSP_RESOURCE_G711; CSFLogDebug( logTag, "%s", " G711"); }
    if ( retVal & AudioCodecMask_LINEAR ) {  codecMask |= cip_sipcc_CodecMask_DSP_RESOURCE_LINEAR; CSFLogDebug( logTag, "%s", " LINEAR" ); }
    if ( retVal & AudioCodecMask_G722 ) {    codecMask |= cip_sipcc_CodecMask_DSP_RESOURCE_G722; CSFLogDebug( logTag, "%s", " G722"); }
    if ( retVal & AudioCodecMask_iLBC )  {   codecMask |= cip_sipcc_CodecMask_DSP_RESOURCE_iLBC; CSFLogDebug( logTag, "%s", " iLBC"); }
    if ( retVal & AudioCodecMask_iSAC )   {  codecMask |= cip_sipcc_CodecMask_DSP_RESOURCE_iSAC; CSFLogDebug( logTag, "%s", " iSAC "); }

    CSFLogDebug( logTag, "%s(codec_mask = %X)", fname, codecMask);
    return codecMask;
#else
  int codecMask = VcmSIPCCBinding::getAudioCodecs();
  CSFLogDebug(logTag, "GetAudioCodecList returning %X", codecMask);

  return codecMask;
#endif
}

/**
 * Get current list of video codec that could be used
 * @param request_type -
 * The request_type should be VCM_DECODEONLY/ENCODEONLY/FULLDUPLEX/IGNORE
 *
 * @return A bit mask should be returned that specifies the list of the audio
 * codecs. The bit mask should conform to the defines in this file.
 * #define VCM_RESOURCE_G711     0x00000001
 * #define VCM_RESOURCE_G729A    0x00000002
 * ....
 */

#ifndef DSP_H264
#define DSP_H264        0x00000001
#endif

#ifndef DSP_H263
#define DSP_H263        0x00000002
#endif

int vcmGetVideoCodecList(int request_type)
{
// Direct access to media layer replaced by locally stored codecMask
// set in PeerConnectionImpl
#if 0
    const char fname[] = "vcmGetVideoCodecList";
    int retVal = 0;
    int codecMask = 0;

    CSFLogDebug( logTag, "%s(request_type = %d)", fname, request_type);

    retVal = VcmSIPCCBinding::getVideoTermination() ? VcmSIPCCBinding::getVideoTermination()->getCodecList( map_codec_request_type(request_type) ) : 0;

    if ( retVal & VideoCodecMask_H264 )    codecMask |= DSP_H264;
    if ( retVal & VideoCodecMask_H263 )    codecMask |= DSP_H263;

    CSFLogDebug( logTag, "%s(codec_mask = %X)", fname, codecMask);

    //return codecMask;
    return VCM_CODEC_RESOURCE_H264;
#else
  // Control if H264 is available and priority:
  // If hardware codecs are available (VP8 or H264), use those as a preferred codec
  // (question: on all platforms?)
  // If OpenH264 is available, use that at lower priority to VP8
  // (question: platform software or OS-unknown-impl codecs?  (Win8.x, etc)
  // Else just use VP8 software

    int codecMask;
    switch (request_type) {
      case VCM_DSP_FULLDUPLEX_HW:
        codecMask = VcmSIPCCBinding::getVideoCodecsHw();
        break;
      case VCM_DSP_FULLDUPLEX_GMP:
        codecMask = VcmSIPCCBinding::getVideoCodecsGmp();
        break;
      default: // VCM_DSP_FULLDUPLEX
        codecMask = VcmSIPCCBinding::getVideoCodecs();
        break;
    }
    CSFLogDebug(logTag, "GetVideoCodecList returning %X", codecMask);
    return codecMask;
#endif
}

/**
 * Get supported H.264 video packetization modes
 * @return mask of supported video packetization modes for H.264. Value returned
 * must be 1 to 3 (bit 0 is mode 0, bit 1 is mode 1.
 * Bit 2 (Mode 2) is not supported yet.
 */
int vcmGetH264SupportedPacketizationModes()
{
#ifdef MOZ_WEBRTC_OMX
  return VCM_H264_MODE_1;
#else
  return VCM_H264_MODE_0|VCM_H264_MODE_1;
#endif
}

/**
 * Get supported H.264 profile-level-id
 * @return supported profile-level-id value
 */
uint32_t vcmGetVideoH264ProfileLevelID()
{
  // constrained baseline level 1.2
  // XXX make variable based on openh264 and OMX support
#ifdef MOZ_WEBRTC_OMX
  // Max resolution CIF; we should include max-mbps
  return 0x42E00C;
#else
  // XXX See bug 1043515 - we may want to support a higher profile than
  // 1.3, depending on hardware(?)
  return 0x42E00D;
#endif
}

/**
 *  MEDIA control received from far end on signaling path
 *
 *  @param call_handle - call_handle of the call
 *  @param to_encoder - the control request received
 *        Only FAST_PICTURE_UPDATE is supported
 *
 *  @return  void
 *
 */

void vcmMediaControl(cc_call_handle_t  call_handle, vcm_media_control_to_encoder_t to_encoder)
{
    if ( to_encoder == VCM_MEDIA_CONTROL_PICTURE_FAST_UPDATE )
    {
        StreamObserver* obs = VcmSIPCCBinding::getStreamObserver();
        if (obs != nullptr)
        {
                obs->sendIFrame(call_handle);
        }
    }
}

/**
 * Get the rx/tx stream statistics associated with the call.
 *
 * @param[in]  mcap_id  - media type (audio/video)
 * @param[in]  group_id - group id of the stream
 * @param[in]  stream_id - stram id of the stream
 * @param[in]  call_handle - call identifier
 * @param[out] rx_stats - ptr to the rx field in the stats struct
 * @param[out] tx_stats - ptr to the tx field in the stats struct
 *
 */

int vcmGetRtpStats(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle,
        char *rx_stats,
        char *tx_stats)
{
    rx_stats[0] = '\0';
    tx_stats[0] = '\0';
    return 0;
}

/**
 *  specifies DSCP marking for RTCP streams
 *
 *  @param group_id - call_handle of the call
 *  @param dscp - the DSCP value to be used
 *
 *  @return  void
 */

void vcmSetRtcpDscp(cc_groupid_t group_id, int dscp)
{
    // do nothing
}

/**
 *
 * The wlan interface puts into unique situation where call control
 * has to allocate the worst case bandwith before creating a
 * inbound or outbound call. The function call will interface through
 * media API into wlan to get the call bandwidth. The function
 * return is asynchronous and will block till the return media
 * callback signals to continue the execution.
 *
 * @note If not using WLAN interface simply return true
 *
 * @return true if the bandwidth can be allocated else false.
 */

cc_boolean vcmAllocateBandwidth(cc_call_handle_t  call_handle, int sessions)
{
    return(TRUE);
}

/**
 *
 * Free the bandwidth allocated for this call
 * using the vcmAllocateBandwidth API
 *
 * @note  If not using WLAN provide a stub
 */

void vcmRemoveBandwidth(cc_call_handle_t  call_handle)
{
    //do nothing
}

/**
 * @brief vcmActivateWlan
 *
 * Free the bandwidth allocated for this call
 * using the vcmAllocateBandwidth API
 *
 * @note If not using WLAN provide a stub
 */

void vcmActivateWlan(cc_boolean is_active)
{
    // do nothing
}

/**
 *  free the media pointer allocated in vcm_negotiate_attrs method
 *
 *  @param ptr - pointer to be freed
 *
 *  @return  void
 */
void vcmFreeMediaPtr(void *ptr)
{
  free(ptr);
}

/**
 * Verify if the SDP attributes for the requested video codec are acceptable
 *
 * This method is called for video codecs only. This method should parse the
 * Video SDP attributes using the SDP helper API and verify if received
 * attributes are acceptable. If the attributes are acceptable any attribute
 * values if needed by vcmTxStart method should be bundled in the desired
 * structure and its pointer should be returned in rccappptr. This opaque
 * pointer shall be provided again when vcmTxStart is invoked.
 *
 * @param [in] media_type - codec for which we are negotiating
 * @param [in] sdp_p - opaque SDP pointer to be used via SDP helper APIs
 * @param [in] level - Parameter to be used with SDP helper APIs
 * @param [out] rcapptr - variable to return the allocated attrib structure
 *
 * @return cc_boolean - true if attributes are accepted false otherwise
 */

cc_boolean vcmCheckAttribs(cc_uint32_t media_type, void *sdp_p, int level,
                           int remote_pt, void **rcapptr)
{
    CSFLogDebug( logTag, "vcmCheckAttribs(): media=%d", media_type);

    cc_uint16_t     temp;
    const char      *ptr;
    uint32_t        t_uint;
    struct VideoCodecConfigH264 *rcap;

    *rcapptr = nullptr;

    int fmtp_inst = ccsdpAttrGetFmtpInst(sdp_p, level, remote_pt);
    if (fmtp_inst < 0) {
      return TRUE;
    }

    switch (media_type)
    {
    case RTP_VP8:
        return TRUE;

    case RTP_H264_P0:
    case RTP_H264_P1:

        rcap = (struct VideoCodecConfigH264 *) cpr_malloc( sizeof(struct VideoCodecConfigH264) );
        if ( rcap == nullptr )
        {
            CSFLogDebug( logTag, "vcmCheckAttribs(): Malloc Failed for rcap");
            return FALSE;
        }
        memset( rcap, 0, sizeof(struct VideoCodecConfigH264) );

        if ( (ptr = ccsdpAttrGetFmtpParamSets(sdp_p, level, 0, fmtp_inst)) != nullptr )
        {
            memset(rcap->sprop_parameter_sets, 0, csf_countof(rcap->sprop_parameter_sets));
            sstrncpy(rcap->sprop_parameter_sets, ptr, csf_countof(rcap->sprop_parameter_sets));
        }

        if ( ccsdpAttrGetFmtpPackMode(sdp_p, level, 0, fmtp_inst, &temp) == SDP_SUCCESS )
        {
            rcap->packetization_mode = temp;
        }

        if ( (ptr = ccsdpAttrGetFmtpProfileLevelId(sdp_p, level, 0, fmtp_inst)) != nullptr )
        {
#ifdef _WIN32
            sscanf_s(ptr, "%x", &rcap->profile_level_id, sizeof(int*));
#else
            sscanf(ptr, "%x", &rcap->profile_level_id);
#endif
        }

        if ( ccsdpAttrGetFmtpMaxMbps(sdp_p, level, 0, fmtp_inst, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_mbps = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxFs(sdp_p, level, 0, fmtp_inst, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_fs = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxCpb(sdp_p, level, 0, fmtp_inst, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_cpb = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxCpb(sdp_p, level, 0, fmtp_inst, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_dpb = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxCpb(sdp_p, level, 0, fmtp_inst, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_br = t_uint;
        }

        rcap->tias_bw = ccsdpGetBandwidthValue(sdp_p, level, fmtp_inst);
        if ( rcap->tias_bw == 0 )
        {
            // received bandwidth of 0 reject this
            free(rcap);
            return FALSE;
        }
        else if ( rcap->tias_bw == SDP_INVALID_VALUE )
        {
            // bandwidth not received pass 0 to ms
            rcap->tias_bw = 0;
        }

        CSFLogDebug( logTag, "vcmCheckAttribs(): Negotiated media attrs\nsprop=%s\npack_mode=%d\nprofile_level_id=%X\nmbps=%d\nmax_fs=%d\nmax_cpb=%d\nmax_dpb=%d\nbr=%d bw=%d\n",
            rcap->sprop_parameter_sets,
            rcap->packetization_mode,
            rcap->profile_level_id,
            rcap->max_mbps,
            rcap->max_fs,
            rcap->max_cpb,
            rcap->max_dpb,
            rcap->max_br,
            rcap->tias_bw);

        *rcapptr = rcap;
        return TRUE;

    default:
        return FALSE;
    }
}

/**
 * Send a DTMF digit
 *
 * This method is called for sending a DTMF tone for the specified duration
 *
 * @param [in] digit - the DTMF digit that needs to be played out.
 * @param [in] duration - duration of the tone
 * @param [in] direction - direction in which the tone needs to be played.
 *
 * @return void
 */
int vcmDtmfBurst(int digit, int duration, int direction)
{
    CSFLogDebug( logTag, "vcmDtmfBurst(): digit=%d duration=%d, direction=%d", digit, duration, direction);
    StreamObserver* obs = VcmSIPCCBinding::getStreamObserver();
    if(obs != nullptr)
        obs->dtmfBurst(digit, duration, direction);
    return 0;
}

/**
 * vcmGetILBCMode
 *
 * This method should return the mode that needs to be used in
 * SDP
 * @return int
 */
int vcmGetILBCMode()
{
    return 0;
}

} // extern "C"

static mozilla::RefPtr<TransportFlow>
vcmCreateTransportFlow(sipcc::PeerConnectionImpl *pc, int level, bool rtcp,
  sdp_setup_type_e setup_type, const char *fingerprint_alg,
  const char *fingerprint) {

  // TODO(ekr@rtfm.com): Check that if the flow already exists the digest
  // is the same. The only way that can happen is if
  //
  // (a) We have an error or
  // (b) The other side bundled but had mismatched digests for each line
  //
  // Not clear that either of these cases matters.
  mozilla::RefPtr<TransportFlow> flow;
  flow = pc->media()->GetTransportFlow(level, rtcp);

  if (!flow) {
    CSFLogDebug(logTag, "Making new transport flow for level=%d rtcp=%s", level, rtcp ? "true" : "false");

    char id[32];
    PR_snprintf(id, sizeof(id), "%s:%d,%s",
                pc->GetHandle().c_str(), level, rtcp ? "rtcp" : "rtp");
    flow = new TransportFlow(id);


    ScopedDeletePtr<TransportLayerIce> ice(
        new TransportLayerIce(pc->GetHandle(), pc->media()->ice_ctx(),
                              pc->media()->ice_media_stream(level-1),
                              rtcp ? 2 : 1));

    ScopedDeletePtr<TransportLayerDtls> dtls(new TransportLayerDtls());

    // RFC 5763 says:
    //
    //   The endpoint MUST use the setup attribute defined in [RFC4145].
    //   The endpoint that is the offerer MUST use the setup attribute
    //   value of setup:actpass and be prepared to receive a client_hello
    //   before it receives the answer.  The answerer MUST use either a
    //   setup attribute value of setup:active or setup:passive.  Note that
    //   if the answerer uses setup:passive, then the DTLS handshake will
    //   not begin until the answerer is received, which adds additional
    //   latency. setup:active allows the answer and the DTLS handshake to
    //   occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
    //   party is active MUST initiate a DTLS handshake by sending a
    //   ClientHello over each flow (host/port quartet).
    //

    // setup_type should at this point be either PASSIVE or ACTIVE
    // other a=setup values should have been negotiated out.
    MOZ_ASSERT(setup_type == SDP_SETUP_PASSIVE ||
               setup_type == SDP_SETUP_ACTIVE);
    dtls->SetRole(
      setup_type == SDP_SETUP_PASSIVE ?
      TransportLayerDtls::SERVER : TransportLayerDtls::CLIENT);
    mozilla::RefPtr<DtlsIdentity> pcid = pc->GetIdentity();
    if (!pcid) {
      return nullptr;
    }
    dtls->SetIdentity(pcid);

    unsigned char remote_digest[TransportLayerDtls::kMaxDigestLength];
    size_t digest_len;

    nsresult res = DtlsIdentity::ParseFingerprint(fingerprint,
                                                  remote_digest,
                                                  sizeof(remote_digest),
                                                  &digest_len);
    if (!NS_SUCCEEDED(res)) {
      CSFLogError(logTag, "Could not convert fingerprint");
      return nullptr;
    }

    std::string fingerprint_str(fingerprint_alg);
    // Downcase because SDP is case-insensitive.
    std::transform(fingerprint_str.begin(), fingerprint_str.end(),
                   fingerprint_str.begin(), ::tolower);
    res = dtls->SetVerificationDigest(fingerprint_str, remote_digest, digest_len);
    if (!NS_SUCCEEDED(res)) {
      CSFLogError(logTag, "Could not set remote DTLS digest");
      return nullptr;
    }

    std::vector<uint16_t> srtp_ciphers;
    srtp_ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
    srtp_ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_32);

    res = dtls->SetSrtpCiphers(srtp_ciphers);
    if (!NS_SUCCEEDED(res)) {
      CSFLogError(logTag, "Couldn't set SRTP ciphers");
      return nullptr;
    }

    nsAutoPtr<std::queue<TransportLayer *> > layers(new std::queue<TransportLayer *>);
    layers->push(ice.forget());
    layers->push(dtls.forget());


    // Layers are now owned by the flow.
    // TODO(ekr@rtfm.com): Propagate errors for when this fails.
    // Bug 854518.
    nsresult rv = pc->media()->ice_ctx()->thread()->Dispatch(
        WrapRunnable(flow, &TransportFlow::PushLayers, layers),
        NS_DISPATCH_NORMAL);

    if (NS_FAILED(rv)) {
      return nullptr;
    }

    // Note, this flow may actually turn out to be invalid
    // because PushLayers is async and can fail.
    pc->media()->AddTransportFlow(level, rtcp, flow);
  }
  return flow;
}


/**
 * vcmOnSdpParseError_m
 *
 * This method is called for each parsing error of SDP.  It does not necessarily
 * mean the SDP read was fatal and can be called many times for the same SDP.
 *
 * This function should only be called on the main thread.
 *
 */
static void vcmOnSdpParseError_m(nsAutoPtr<std::string> peerconnection,
                                 nsAutoPtr<std::string> message) {

  sipcc::PeerConnectionWrapper pc(peerconnection->c_str());
  ENSURE_PC_NO_RET(pc, peerconnection->c_str());

  pc.impl()->OnSdpParseError(message->c_str());
}


/**
 * vcmOnSdpParseError
 *
 * Dispatch the static version of this function on the main thread.
 * The string parameters are autoptr in order to survive the DISPATCH_NORMAL
 *
 */
int vcmOnSdpParseError(const char *peerconnection, const char *message) {
  MOZ_ASSERT(peerconnection);
  MOZ_ASSERT(message);
  nsAutoPtr<std::string> peerconnectionDuped(new std::string(peerconnection));
  nsAutoPtr<std::string> messageDuped(new std::string(message));

  // Now DISPATCH_NORMAL with the duped strings
  nsresult rv = VcmSIPCCBinding::getMainThread()->Dispatch(
      WrapRunnableNM(&vcmOnSdpParseError_m,
                   peerconnectionDuped,
                   messageDuped),
      NS_DISPATCH_NORMAL);

  if (!NS_SUCCEEDED(rv)) {
    CSFLogError( logTag, "%s(): Could not dispatch to main thread", __FUNCTION__);
    return VCM_ERROR;
  }

  return 0;
}

/**
 * vcmDisableRtcpComponent_m
 *
 * If we are doing rtcp-mux we need to disable component number 2 in the ICE
 * layer.  Otherwise we will wait for it to connect when it is unused
 */
static int vcmDisableRtcpComponent_m(const char *peerconnection, int level) {
#ifdef MOZILLA_INTERNAL_API
  MOZ_ASSERT(NS_IsMainThread());
#endif
  MOZ_ASSERT(level > 0);

  sipcc::PeerConnectionWrapper pc(peerconnection);
  ENSURE_PC(pc, VCM_ERROR);

  CSFLogDebug( logTag, "%s: disabling rtcp component %d", __FUNCTION__, level);
  mozilla::RefPtr<NrIceMediaStream> stream = pc.impl()->media()->
    ice_media_stream(level-1);
  MOZ_ASSERT(stream);
  if (!stream) {
    return VCM_ERROR;
  }

  // The second component is for RTCP
  nsresult res = stream->DisableComponent(2);
  MOZ_ASSERT(NS_SUCCEEDED(res));
  if (!NS_SUCCEEDED(res)) {
    return VCM_ERROR;
  }

  return 0;
}

/**
 * vcmDisableRtcpComponent
 *
 * If we are doing rtcp-mux we need to disable component number 2 in the ICE
 * layer.  Otherwise we will wait for it to connect when it is unused
 */
int vcmDisableRtcpComponent(const char *peerconnection, int level) {
  int ret;
  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmDisableRtcpComponent_m,
                        peerconnection,
                        level,
                        &ret));
  return ret;
}

static short vcmGetVideoPref_m(uint16_t codec,
                               const char *pref,
                               int32_t *ret) {
  nsCOMPtr<nsIPrefBranch> branch = VcmSIPCCBinding::getPrefBranch();
  if (branch && NS_SUCCEEDED(branch->GetIntPref(pref, ret))) {
    return 0;
  }
  return VCM_ERROR;
}

short vcmGetVideoMaxFs(uint16_t codec,
                       int32_t *max_fs) {
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetVideoPref_m,
                        codec,
                        "media.navigator.video.max_fs",
                        max_fs,
                        &ret));
  return ret;
}

short vcmGetVideoMaxFr(uint16_t codec,
                       int32_t *max_fr) {
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetVideoPref_m,
                        codec,
                        "media.navigator.video.max_fr",
                        max_fr,
                        &ret));
  return ret;
}

short vcmGetVideoMaxBr(uint16_t codec,
                       int32_t *max_br) {
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetVideoPref_m,
                        codec,
                        "media.navigator.video.h264.max_br",
                        max_br,
                        &ret));
  return ret;
}

short vcmGetVideoMaxMbps(uint16_t codec,
                       int32_t *max_mbps) {
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetVideoPref_m,
                        codec,
                        "media.navigator.video.h264.max_mbps",
                        max_mbps,
                        &ret));
  if (ret == VCM_ERROR) {
#ifdef MOZ_WEBRTC_OMX
    // Level 1.2; but let's allow CIF@30 or QVGA@30+ by default
    *max_mbps = 11880;
    ret = 0;
#endif
  }
  return ret;
}

short vcmGetVideoPreferredCodec(int32_t *preferred_codec) {
  short ret;

  mozilla::SyncRunnable::DispatchToThread(VcmSIPCCBinding::getMainThread(),
      WrapRunnableNMRet(&vcmGetVideoPref_m,
                        (uint16_t)0,
                        "media.navigator.video.preferred_codec",
                        preferred_codec,
                        &ret));
  return ret;
}
