/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CC_Common.h"

#include "CSFLogStream.h"

#include "CSFMediaProvider.h"
#include "CSFAudioTermination.h"
#include "CSFVideoTermination.h"
#include "VcmSIPCCBinding.h"
#include "csf_common.h"

#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include "ccsdp.h"
#include "vcm.h"
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

}//end extern "C"

static const char* logTag = "VcmSipccBinding";

typedef enum {
    CC_AUDIO_1,
    CC_VIDEO_1
} cc_media_cap_name;

#define SIPSDP_ILBC_MODE20 20

/* static */

using namespace CSF;

VcmSIPCCBinding * VcmSIPCCBinding::_pSelf = NULL;


// The media provider passsed in here will be owned by VcmSIPCCBinding, and so it destroys
// it later.
VcmSIPCCBinding::VcmSIPCCBinding (MediaProvider *mp)
  : pMediaProvider(mp),
    streamObserver(NULL)
{
    delete _pSelf;//delete is NULL safe, so I don't need to check if it's NULL
    _pSelf = this;
}

VcmSIPCCBinding::~VcmSIPCCBinding ()
{
    assert(_pSelf != NULL);
    pMediaProvider->shutdown();
    delete pMediaProvider;
    pMediaProvider = NULL;
    _pSelf = NULL;
}

void VcmSIPCCBinding::setStreamObserver(StreamObserver* obs)
{
	streamObserver = obs;
}

/* static */
StreamObserver * VcmSIPCCBinding::getStreamObserver()
{
    if (_pSelf != NULL)
    	return _pSelf->streamObserver;

    return NULL;
}

void VcmSIPCCBinding::setMediaProviderObserver(MediaProviderObserver* obs)
{
	mediaProviderObserver = obs;
}


MediaProviderObserver * VcmSIPCCBinding::getMediaProviderObserver()
{
    if (_pSelf != NULL)
    	return _pSelf->mediaProviderObserver;

    return NULL;
}


/* static */
AudioTermination * VcmSIPCCBinding::getAudioTermination()
{
    if ((_pSelf == NULL) || (_pSelf->pMediaProvider == NULL))
    {
        return NULL;
    }

    return _pSelf->pMediaProvider->getAudioTermination();
}

/* static */
VideoTermination * VcmSIPCCBinding::getVideoTermination()
{
    if ((_pSelf == NULL) || (_pSelf->pMediaProvider == NULL))
    {
        return NULL;
    }

    return _pSelf->pMediaProvider->getVideoTermination();
}

/* static */
AudioControl * VcmSIPCCBinding::getAudioControl()
{
    if ((_pSelf == NULL) || (_pSelf->pMediaProvider == NULL))
    {
        return NULL;
    }

    return _pSelf->pMediaProvider->getAudioControl();
}

/* static */
VideoControl * VcmSIPCCBinding::getVideoControl()
{
    if ((_pSelf == NULL) || (_pSelf->pMediaProvider == NULL))
    {
        return NULL;
    }

    return _pSelf->pMediaProvider->getVideoControl();
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

#define    MAX_SPROP_LEN    32

#define VCM_ERROR -1

struct h264_video
{
    char       sprop_parameter_set[MAX_SPROP_LEN];
    int        packetization_mode;
    int        profile_level_id;
    int        max_mbps;
    int        max_fs;
    int        max_cpb;
    int        max_dpb;
    int        max_br;
    int        tias_bw;
};

static RingMode
map_ring_mode (vcm_ring_mode_t mode)
{
    switch ( mode )
    {
    case VCM_INSIDE_RING:
        return RingMode_INSIDE_RING;
    case VCM_OUTSIDE_RING:
        return RingMode_OUTSIDE_RING;
    case VCM_FEATURE_RING:
        return RingMode_FEATURE_RING;
    case VCM_BELLCORE_DR1:
        return RingMode_BELLCORE_DR1;
    case VCM_BELLCORE_DR2:
        return RingMode_BELLCORE_DR2;
    case VCM_BELLCORE_DR3:
        return RingMode_BELLCORE_DR3;
    case VCM_BELLCORE_DR4:
        return RingMode_BELLCORE_DR4;
    case VCM_BELLCORE_DR5:
        return RingMode_BELLCORE_DR5;
    case VCM_FLASHONLY_RING:
        return RingMode_FLASHONLY_RING;
    case VCM_STATION_PRECEDENCE_RING:
        return RingMode_PRECEDENCE_RING;
    default:
        CSFLogDebugS( logTag, "map_ring_mode(): Wrong ringmode passed");
        return RingMode_INSIDE_RING;
    }
}

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
 *  Function:map_VCM_Media_Payload_type
 *
 *  Parameters:payload
 *
 *  Description: Converts VCM payload type to MediaManager Payload defs.
 *
 *  Returns:payload type corresponding to VCM payload
 *
 */

#define MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCMPayloadItem, CIPPayloadItem)\
  vcmToCIP_Mappings[VCMPayloadItem] = CIPPayloadItem

static int
map_VCM_Media_Payload_type( vcm_media_payload_type_t payload )
{
    static bool mappingInitialised = false;
    static int vcmToCIP_Mappings[VCM_Media_Payload_Max] = { };

    if (!mappingInitialised)
    {
        int numElements = csf_countof(vcmToCIP_Mappings);

        std::fill_n(vcmToCIP_Mappings, numElements, cip_mmgr_MediaDefinitions_MEDIA_TYPE_NONSTANDARD);

        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_NonStandard, cip_mmgr_MediaDefinitions_MEDIA_TYPE_NONSTANDARD);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G711Alaw64k, cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ALAW64K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G711Alaw56k, cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ALAW56K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G711Ulaw64k, cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ULAW64K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G711Ulaw56k, cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ULAW56K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G722_64k,    cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_64K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G722_56k,    cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_56K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G722_48k,    cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_48K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_ILBC20,      cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC20 | (VCM_Media_Payload_ILBC20 & 0XFFFF0000));
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_ILBC30,      cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC30 | (cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC30 & 0XFFFF0000));
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G7231,       cip_mmgr_MediaDefinitions_MEDIA_TYPE_G7231_5P3K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G728,        cip_mmgr_MediaDefinitions_MEDIA_TYPE_G728);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G729,        cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G729AnnexA,  cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXA);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_Is11172AudioCap, cip_mmgr_MediaDefinitions_MEDIA_TYPE_IS11172AUDIOCAP);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_Is13818AudioCap, cip_mmgr_MediaDefinitions_MEDIA_TYPE_IS13818AUDIOCAP);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G729AnnexB,             cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXB);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_GSM_Full_Rate,          cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_FULL_RATE);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_GSM_Half_Rate,          cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_HALF_RATE);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_GSM_Enhanced_Full_Rate, cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_ENHANCED_FULL_RATE);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_Wide_Band_256k,         cip_mmgr_MediaDefinitions_MEDIA_TYPE_WIDE_BAND_256K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_Data64,                 cip_mmgr_MediaDefinitions_MEDIA_TYPE_DATA64);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_Data56,                 cip_mmgr_MediaDefinitions_MEDIA_TYPE_DATA56);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_GSM,             cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_ActiveVoice,             cip_mmgr_MediaDefinitions_MEDIA_TYPE_ACTIVEVOICE);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G726_32K,             cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_32K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G726_24K,             cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_24K);
        MAKE_VCM_MEDIA_PAYLOAD_MAP_ENTRY(VCM_Media_Payload_G726_16K,             cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_16K);

        mappingInitialised = true;
    }

    int vcmIndex = payload & 0XFFFF;

    switch (vcmIndex)
    {
    case VCM_Media_Payload_ILBC20:
        return ((payload & 0XFFFF0000) | cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC20);
    case VCM_Media_Payload_ILBC30:
        return ((payload & 0XFFFF0000) | cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC30);
    case VCM_Media_Payload_H263:
        return ((payload & 0XFFFF0000) | cip_mmgr_MediaDefinitions_MEDIA_TYPE_H263);
    case VCM_Media_Payload_H264:
        return ((payload & 0XFFFF0000) | RTP_H264_P0);
    case VCM_Media_Payload_ISAC:
        return ((payload & 0XFFFF0000) | cip_mmgr_MediaDefinitions_MEDIA_TYPE_ISAC);
    default:
        //use the static array
        if (vcmIndex < VCM_Media_Payload_Max)
        {
            return vcmToCIP_Mappings[vcmIndex];
        }

        return cip_mmgr_MediaDefinitions_MEDIA_TYPE_NONSTANDARD;
    }
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
    CSFLogDebug( logTag, "vcmRxAllocPort(): group_id=%d stream_id=%d call_handle=%d port_requested = %d",
        group_id, stream_id, call_handle, port_requested);

    int port = -1;
    bool isVideo = false;
    if(CC_IS_AUDIO(mcap_id))
    {
    	isVideo = false;
        if ( VcmSIPCCBinding::getAudioTermination() != NULL )
            port = VcmSIPCCBinding::getAudioTermination()->rxAlloc( group_id, stream_id, port_requested );
    }
	else if(CC_IS_VIDEO(mcap_id))
	{
		isVideo = true;
        if ( VcmSIPCCBinding::getVideoTermination() != NULL )
            port = VcmSIPCCBinding::getVideoTermination()->rxAlloc( group_id, stream_id, port_requested );
    }

    StreamObserver* obs = VcmSIPCCBinding::getStreamObserver();
    if(obs != NULL)
    	obs->registerStream(call_handle, stream_id, isVideo);

    CSFLogDebug( logTag, "vcmRxAllocPort(): allocated port %d", port);
    *port_allocated = port;
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
 *  @param[in,out] port_allocated   - allocated(reserved) port
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
        if ( VcmSIPCCBinding::getAudioTermination() != NULL )
            *port_allocated = VcmSIPCCBinding::getAudioTermination()->rxOpen( group_id, stream_id,
                                                    port_requested, listen_ip ? listen_ip->u.ip4 : 0,
                                                    (is_multicast != 0) );
        break;
    case CC_VIDEO_1:
        CSFLogDebug( logTag, "%s: video stream", fname);
        if ( VcmSIPCCBinding::getVideoTermination() != NULL )
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
 *  @param[in]    call_handle     - call identifier
 *  @param[in]    payload     - payload type
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
        vcm_media_payload_type_t payload,
        cpr_ip_addr_t *local_addr,
        cc_uint16_t port,
        vcm_crypto_algorithmID algorithmID,
        vcm_crypto_key_t *rx_key,
        vcm_mediaAttrs_t *attrs)
{
    int         pt;
    uint8_t    *key;
    uint8_t    *salt;
    cc_uint16_t    key_len;
    cc_uint16_t    salt_len;
    char        fname[] = "vcmRxStart";

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d payload=%d port=%d algID=%d",
        fname, group_id, call_handle, payload, port, algorithmID);

    if (call_handle == CC_NO_CALL_ID) {
        /* no operation when no call ID */
        return VCM_ERROR;
    }

    /* Set up crypto parameters */
    switch ( algorithmID )
    {
    case VCM_AES_128_COUNTER:
        if ( rx_key == NULL )
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
        /* Give dummy data to avoid passing NULL for key/salt */
        key_len = 0;
        key = (uint8_t *)"";
        salt_len = 0;
        salt = (uint8_t *)"";
        break;
    }

    pt = map_VCM_Media_Payload_type(payload);

    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != NULL )
            return VcmSIPCCBinding::getAudioTermination()->rxStart( group_id, stream_id, DYNAMIC_PAYLOAD_TYPE(pt),
                                                                    attrs->audio.packetization_period, port, attrs->audio.avt_payload_type,
                                                                    map_algorithmID(algorithmID), key, key_len, salt, salt_len,
                                                                    attrs->audio.mixing_mode, attrs->audio.mixing_party );
        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != NULL )
            return VcmSIPCCBinding::getVideoTermination()->rxStart( group_id, stream_id, DYNAMIC_PAYLOAD_TYPE(pt),
                                                                    0,
                                                                    port,
                                                                    0,
                                                                    map_algorithmID(algorithmID), key, key_len, salt, salt_len,
                                                                    0, 0);
        break;

    default:
        break;
    }
    return VCM_ERROR;
}

/**
 *  Close the receive stream.
 *
 *  @param[in]    mcap_id - Media Capability ID
 *  @param[in]    group_id - group identifier that belongs to the stream.
 *  @param[in]    stream_id - stream id of the given media type.
 *  @param[in]    call_handle  - call identifier
 *
 *  @return   None
 *
 */

void vcmRxClose(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle)
{
    char fname[] = "vcmRxClose";

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d", fname, group_id, call_handle);

    if (call_handle == CC_NO_CALL_ID) {
        CSFLogDebugS( logTag, "No CALL ID");
        /* no operation when no call ID */
        return;
    }
    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != NULL )
            VcmSIPCCBinding::getAudioTermination()->rxClose( group_id, stream_id );
        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != NULL )
            VcmSIPCCBinding::getVideoTermination()->rxClose( group_id, stream_id );
        break;

    default:
        break;
    }
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
        if ( VcmSIPCCBinding::getAudioTermination() != NULL )
            VcmSIPCCBinding::getAudioTermination()->rxRelease( group_id, stream_id, port );
    }
    else if(CC_IS_VIDEO(mcap_id))
    {
        if ( VcmSIPCCBinding::getVideoTermination() != NULL )
           VcmSIPCCBinding::getVideoTermination()->rxRelease( group_id, stream_id, port );
    }

    StreamObserver* obs = VcmSIPCCBinding::getStreamObserver();
    if(obs != NULL)
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
        CSFLogDebugS( logTag, "map_tone_type(): WARNING..tone type not mapped.");
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
 *  @param[in]   payload      - payload type
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
        vcm_media_payload_type_t payload,
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
    int         pt;
    uint8_t    *key;
    uint8_t    *salt;
    cc_uint16_t    key_len;
    cc_uint16_t    salt_len;

    char dottedIP[20];
    csf_sprintf(dottedIP, sizeof(dottedIP), "%u.%u.%u.%u",
                (remote_ip_addr->u.ip4 >> 24) & 0xff, (remote_ip_addr->u.ip4 >> 16) & 0xff,
                (remote_ip_addr->u.ip4 >> 8) & 0xff, remote_ip_addr->u.ip4 & 0xff );

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d payload=%d tos=%d local_port=%d remote=%s:%d algID=%d",
        fname, group_id, call_handle, payload, tos, local_port,
        dottedIP, remote_port, algorithmID);

    if (call_handle == CC_NO_CALL_ID) {
        /* no operation when no call ID */
        return VCM_ERROR;
    }

    /* Set up crypto parameters */
    switch ( algorithmID )
    {
    case VCM_AES_128_COUNTER:
        if ( tx_key == NULL )
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
        /* Give dummy data to avoid passing NULL for key/salt */
        key_len  = 0;
        key      = (uint8_t *)"";
        salt_len = 0;
        salt     = (uint8_t *)"";
        break;
    }

    pt = map_VCM_Media_Payload_type(payload);

    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != NULL )
            return VcmSIPCCBinding::getAudioTermination()->txStart( group_id, stream_id, pt,
                                            attrs->audio.packetization_period, (attrs->audio.vad != 0),
                                            tos, dottedIP, remote_port, attrs->audio.avt_payload_type,
                                            map_algorithmID(algorithmID), key, key_len, salt, salt_len,
                                            attrs->audio.mixing_mode, attrs->audio.mixing_party );
        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != NULL )
           return VcmSIPCCBinding::getVideoTermination()->txStart( group_id, stream_id, pt,
                                                                   0, 0, tos, dottedIP, remote_port, 0,
                                                                   map_algorithmID(algorithmID), key, key_len, salt, salt_len, 0, 0 );
        break;

    default:
        break;
    }
    return VCM_ERROR;
}

/**
 *  Close the transmit stream
 *
 *  @param[in] mcap_id - Media Capability ID
 *  @param[in] group_id - identifier of the group to which stream belongs
 *  @param[in] stream_id - stream id of the given media type.
 *  @param[in] call_handle  - call identifier
 *
 *  @return     void
 */

void vcmTxClose(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle)
{
    const char fname[] = "vcmTxClose";

    CSFLogDebug( logTag, "%s: group_id=%d call_handle=%d", fname, group_id, call_handle);

    if (call_handle == CC_NO_CALL_ID) {
        /* no operation when no call ID */
        return;
    }

    switch ( mcap_id )
    {
    case CC_AUDIO_1:
        if ( VcmSIPCCBinding::getAudioTermination() != NULL )
            VcmSIPCCBinding::getAudioTermination()->txClose( group_id, stream_id);
        break;

    case CC_VIDEO_1:
        if ( VcmSIPCCBinding::getVideoTermination() != NULL )
           VcmSIPCCBinding::getVideoTermination()->txClose( group_id, stream_id);
        break;

    default:
        break;
    }
}

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
}

/**
 * Get max supported H.264 video packetization mode. 
 * @return maximum supported video packetization mode for H.264. Value returned
 * must be 0 or 1. Value 2 is not supported yet.
 */
int vcmGetVideoMaxSupportedPacketizationMode()
{
    return 0;
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
    	if (obs != NULL)
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

cc_boolean vcmCheckAttribs(cc_uint32_t media_type, void *sdp_p, int level, void **rcapptr)
{
    CSFLogDebug( logTag, "vcmCheckAttribs(): media=%d", media_type);

    cc_uint16_t     temp;
    const char      *ptr;
    uint32_t        t_uint;
    struct h264_video *rcap;

    *rcapptr = NULL;

    switch (media_type)
    {
    case RTP_H264_P0:
    case RTP_H264_P1:

        rcap = (struct h264_video *) malloc( sizeof(struct h264_video) );
        if ( rcap == NULL )
        {
            CSFLogDebugS( logTag, "vcmCheckAttribs(): Malloc Failed for rcap");
            return FALSE;
        }
        memset( rcap, 0, sizeof(struct h264_video) );

        if ( (ptr = ccsdpAttrGetFmtpParamSets(sdp_p, level, 0, 1)) != NULL )
        {
            memset(rcap->sprop_parameter_set, 0, csf_countof(rcap->sprop_parameter_set));
            csf_strcpy(rcap->sprop_parameter_set, csf_countof(rcap->sprop_parameter_set), ptr);                     
        }

        if ( ccsdpAttrGetFmtpPackMode(sdp_p, level, 0, 1, &temp) == SDP_SUCCESS )
        {
            rcap->packetization_mode = temp;
        }

        if ( (ptr = ccsdpAttrGetFmtpProfileLevelId(sdp_p, level, 0, 1)) != NULL )
        {
#ifdef _WIN32
            sscanf_s(ptr, "%x", &rcap->profile_level_id, sizeof(int*));
#else
            sscanf(ptr, "%x", &rcap->profile_level_id);
#endif
        }

        if ( ccsdpAttrGetFmtpMaxMbps(sdp_p, level, 0, 1, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_mbps = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxFs(sdp_p, level, 0, 1, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_fs = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxCpb(sdp_p, level, 0, 1, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_cpb = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxCpb(sdp_p, level, 0, 1, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_dpb = t_uint;
        }

        if ( ccsdpAttrGetFmtpMaxCpb(sdp_p, level, 0, 1, &t_uint) == SDP_SUCCESS )
        {
            rcap->max_br = t_uint;
        }

        rcap->tias_bw = ccsdpGetBandwidthValue(sdp_p, level, 1);
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
            rcap->sprop_parameter_set,
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
 * Add Video attributes in the offer/answer SDP
 *
 * This method is called for video codecs only. This method should populate the
 * Video SDP attributes using the SDP helper API
 *
 * @param [in] sdp_p - opaque SDP pointer to be used via SDP helper APIs
 * @param [in] level - Parameter to be used with SDP helper APIs
 * @param [in] media_type - codec for which the SDP attributes are to be populated
 * @param [in] payload_number - RTP payload type used for the SDP
 * @param [in] isOffer - cc_boolean indicating we are encoding an offer or an aswer
 *
 * @return void
 */
void vcmPopulateAttribs(void *sdp_p, int level, cc_uint32_t media_type,
                          cc_uint16_t payload_number, cc_boolean isOffer)
{
    CSFLogDebug( logTag, "vcmPopulateAttribs(): media=%d PT=%d, isOffer=%d", media_type, payload_number, isOffer);
    uint16_t a_inst;//, a_inst2, a_inst3, a_inst4;
    int profile;
    char profile_level_id[MAX_SPROP_LEN];

    switch (media_type)
    {
    case RTP_H264_P0:
    case RTP_H264_P1:

        if ( ccsdpAddNewAttr(sdp_p, level, 0, SDP_ATTR_FMTP, &a_inst) != SDP_SUCCESS ) return;

        (void) ccsdpAttrSetFmtpPayloadType(sdp_p, level, 0, a_inst, payload_number);

        //(void) sdp_attr_set_fmtp_pack_mode(sdp_p, level, 0, a_inst, 1 /*packetization_mode*/);
        //(void) sdp_attr_set_fmtp_parameter_sets(sdp_p, level, 0, a_inst, "J0KAFJWgUH5A,KM4H8n==");    // NAL units 27 42 80 14 95 a0 50 7e 40 28 ce 07 f2

        //profile = 0x42E000 + H264ToSDPLevel( vt_GetClientProfileLevel() );
        profile = 0x42E00C;
        csf_sprintf(profile_level_id, MAX_SPROP_LEN, "%X", profile);
        (void) ccsdpAttrSetFmtpProfileLevelId(sdp_p, level, 0, a_inst, profile_level_id);

        //(void) sdp_attr_set_fmtp_max_mbps(sdp_p, level, 0, a_inst, max_mbps);
        //(void) sdp_attr_set_fmtp_max_fs(sdp_p, level, 0, a_inst, max_fs);
        //(void) sdp_attr_set_fmtp_max_cpb(sdp_p, level, 0, a_inst, max_cpb);
        //(void) sdp_attr_set_fmtp_max_dpb(sdp_p, level, 0, a_inst, max_dpb);
        //(void) sdp_attr_set_fmtp_max_br(sdp_p, level, 0, a_inst, max_br);
        //(void) sdp_add_new_bw_line(sdp_p, level, &a_inst);
        //(void) sdp_set_bw(sdp_p, level, a_inst, SDP_BW_MODIFIER_TIAS, tias_bw);

        break;

    default:
        break;
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
    if(obs != NULL)
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
