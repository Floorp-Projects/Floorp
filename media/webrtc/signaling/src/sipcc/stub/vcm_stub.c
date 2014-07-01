/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @mainpage VCM APIs.
 *
 *  @section Introduction
 *  This module contains command APIs to the media layer
 */

/**
 * @file   vcm.h
 * @brief  APIs to interface with the Media layer.
 *
 * This file contains API that interface to the media layer on the platform.
 * The following APIs need to be implemented to have the sip stack interact
 * and issue commands to the media layer.
 */

#include "cpr_types.h"
#include "vcm.h"
#include "rtp_defs.h"
#include "ccsdp.h"


/**
 *  The initialization of the VCM module
 *
 */
void vcmInit()
{
    return ;
}

/**
 *   Should we remove this from external API
 *
 *  @param[in] mcap_id - group identifier to which stream belongs.
 *  @param[in]     group_id         - group identifier
 *  @param[in]     cc_stream_id        - stream identifier
 *  @param[in]     call_handle      - call handle
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

short vcmRxOpen(cc_mcapid_t mcap_id, cc_groupid_t group_id, cc_streamid_t stream_id,  cc_call_handle_t call_handle,
                  uint16_t port_requested, cpr_ip_addr_t *listen_ip,
                  boolean is_multicast, int *port_allocated)
{
    return 0;
}
/*!
 *  should we remove from external API
 *
 *  @param[in]  mcap_id - Media Capability ID
 *  @param[in]  group_id - group to which stream belongs
 *  @param[in]  cc_stream_id - stream identifier
 *  @param[in]  call_handle - call handle
 *
 *  @return  zero(0) for success otherwise, ERROR for failure
 *
 */

short vcmTxOpen(cc_mcapid_t mcap_id, cc_groupid_t group_id, cc_streamid_t stream_id, cc_call_handle_t call_handle)
{
    return 0;
}

/*!
 *  Allocate(Reserve) a receive port.
 *
 *  @param[in]  mcap_id - Media Capability ID
 *  @param[in]  group_id - group identifier to which stream belongs.
 *  @param[in]  cc_stream_id - stream identifier
 *  @param[in]  call_handle  - call handle
 *  @param[in]  port_requested - port requested (if zero -> give any)
 *  @param[out]  port_allocated - port that was actually allocated.
 *
 *  @return    void
 *
 */

void vcmRxAllocPort(cc_mcapid_t mcap_id, cc_groupid_t group_id, cc_streamid_t stream_id,  cc_call_handle_t call_handle,
                       uint16_t port_requested, int *port_allocated)
{
    return;
}

/*!
 *  Release the allocated port
 * @param[in] mcap_id   - media capability id (0 is audio)
 * @param[in] group_id  - group identifier
 * @param[in] cc_stream_id - stream identifier
 * @param[in] call_handle   - call handle
 * @param[in] port     - port to be released
 *
 * @return void
 */

void vcmRxReleasePort(cc_mcapid_t mcap_id, cc_groupid_t group_id,cc_streamid_t stream_id,  cc_call_handle_t call_handle, int port)
{
    return;
}

/*!
 *  Start receive stream
 *  Note: For video calls, for a given call_id there will be
 *        two media lines and the corresponding group_id/cc_stream_id pair.
 *        One RTP session is requested from media server for each
 *        media line(group/stream) i.e. a video call would result in
 *        two rtp_sessions in our session info list created by two
 *        calls to vcm_rx/tx with mcap_id of AUDIO and VIDEO respectively.
 *
 *  @param[in]    mcap_id     - media type id
 *  @param[in]    group_id    - group identifier associated with the stream
 *  @param[in]    cc_stream_id   - id of the stream one per each media line
 *  @param[in]    call_handle     - call handle
 *  @param[in]    payload     - payload type
 *  @param[in]    local_addr  - local ip address to use.
 *  @param[in]    port        - local port (receive)
 *  @param[in]    algorithmID - crypto alogrithm ID
 *  @param[in]    rx_key      - rx key used when algorithm ID is encrypting
 *  @param[in]    attrs       - media attributes
 *
 *  @return   zero(0) for success otherwise, -1 for failure
 *
 */

int vcmRxStart(cc_mcapid_t mcap_id, cc_groupid_t group_id, cc_streamid_t
               stream_id, cc_call_handle_t call_handle,
               const vcm_payload_info_t *payload, cpr_ip_addr_t *local_addr,
               uint16_t port, vcm_crypto_algorithmID algorithmID,
               vcm_crypto_key_t *rx_key, vcm_mediaAttrs_t *attrs)
{
    return 0;
}

/**
 *  start tx stream
 *  Note: For video calls, for a given call_id there will be
 *        two media lines and the corresponding group_id/cc_stream_id pair.
 *        One RTP session is requested from media server for each
 *        media line(group/stream) i.e. a video call would result in
 *        two rtp_sessions in our session info list created by two
 *        calls to vcm_rx/tx with mcap_id of AUDIO and VIDEO respectively.
 *
 *  @param[in]   mcap_id      - media cap id
 *  @param[in]   group_id     - group identifier to which the stream belongs
 *  @param[in]   cc_stream_id    - stream id of the given media type.
 *  @param[in]   call_handle  - call handle
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
 *  Returns: zero(0) for success otherwise, ERROR for failure
 *
 */

int vcmTxStart(cc_mcapid_t mcap_id, cc_groupid_t group_id,
               cc_streamid_t stream_id,  cc_call_handle_t call_handle,
               const vcm_payload_info_t *payload, short tos,
               cpr_ip_addr_t *local_addr, uint16_t local_port,
               cpr_ip_addr_t *remote_ip_addr, uint16_t remote_port,
               vcm_crypto_algorithmID algorithmID, vcm_crypto_key_t *tx_key,
               vcm_mediaAttrs_t *attrs)
{
    return 0;
}

/*!
 *  Close the receive stream.
 *
 *  @param[in]    mcap_id - Media Capability ID
 *  @param[in]    group_id - group identifier that belongs to the stream.
 *  @param[in]    cc_stream_id - stream id of the given media type.
 *  @param[in]    call_handle  - call handle
 *
 *  @return   None
 *
 */

void vcmRxClose(cc_mcapid_t mcap_id, cc_groupid_t group_id,cc_streamid_t stream_id,  cc_call_handle_t call_handle)
{
    return;
}

/**
 *  Close the transmit stream
 *
 *  @param[in] mcap_id - Media Capability ID
 *  @param[in] group_id - identifier of the group to which stream belongs
 *  @param[in] cc_stream_id - stream id of the given media type.
 *  @param[in] call_handle  - call handle
 *
 *  @return     void
 */

void vcmTxClose(cc_mcapid_t mcap_id, cc_groupid_t group_id, cc_streamid_t stream_id, cc_call_handle_t call_handleS)
{
    return;
}

/**
 *  To be Deprecated
 *  This may be needed to be implemented if the DSP doesn't automatically enable the side tone
 *  The stack will make a call to this method based on the call state. Provide a stub if this is not needed.
 *
 *  @param[in] side_tone - boolean to enable/disable side tone
 *
 *  @return void
 *
 */
void vcmEnableSidetone(uint16_t side_tone)
{
    return;
}

/**
 *  Start a tone (continuous)
 *
 *  Parameters:
 *  @param[in] tone       - tone type
 *  @param[in] alert_info - alertinfo header
 *  @param[in] call_handle    - call handle
 *  @param[in] group_id - identifier of the group to which stream belongs
 *  @param[in] cc_stream_id   - stream identifier.
 *  @param[in] direction  - network, speaker, both
 *
 *  @return void
 *
 */

void vcmToneStart(vcm_tones_t tone, short alert_info, cc_call_handle_t call_handle, cc_groupid_t group_id,
                    cc_streamid_t stream_id, uint16_t direction)
{
    return;
}

/**
 * Plays a short tone. uses the open audio path.
 * If no audio path is open, plays on speaker.
 *
 * @param[in] tone       - tone type
 * @param[in] alert_info - alertinfo header
 * @param[in] call_handle - call handle
 * @param[in] group_id - identifier of the group to which stream belongs
 * @param[in] cc_stream_id   - stream identifier.
 * @param[in] direction  - network, speaker, both
 *
 * @return void
 */

void vcmToneStartWithSpeakerAsBackup(vcm_tones_t tone, short alert_info, cc_call_handle_t call_handle, cc_groupid_t group_id,
                    cc_streamid_t stream_id, uint16_t direction)
{
    return;
}

/**
 *  Stop the tone being played.
 *
 *  Description: Stop the tone being played currently
 *
 *
 * @param[in] tone - tone to be stopeed
 * @param[in] group_id - associated stream's group
 * @param[in] cc_stream_id - associated stream id
 * @param[in] call_handle - the context (call) for this tone.
 *
 * @return void
 *
 */

void vcmToneStop(vcm_tones_t tone, cc_groupid_t group_id, cc_streamid_t cc_stream_id, cc_call_handle_t call_handle)
{
    return;
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

void vcmControlRinger(vcm_ring_mode_t ringMode, short once,
                        boolean alert_info, int line, cc_callid_t call_id)
{
    return;
}


/**
 * Enable / disable speaker
 *
 * @param[in] state - true -> enable speaker, false -> disable speaker
 *
 * @return void
 */

void vcmSetSpeakerMode(boolean state)
{
    return;
}

/**
 * Get current list of audio codec that could be used
 * @param request_type - sendonly/recvonly/sendrecv
 */

int vcmGetAudioCodecList(int request_type)
{
    return 0;
}
/**
 * Get current list of video codec that could be used
 * @param request_type - sendonly/recvonly/sendrecv
 */

int vcmGetVideoCodecList(int request_type)
{
    return 0;
}

/**
 * Get max supported video packetization mode for H.264 video
 */
/*
int vcmGetVideoMaxSupportedPacketizationMode()
{
    return 0;
}
*/
/**
 * Get the rx/tx stream statistics associated with the call.
 *
 * @param[in]  mcap_id  - media type (audio/video)
 * @param[in]  group_id - group id of the stream
 * @param[in]  cc_stream_id - stram id of the stream
 * @param[in]  call_handle - call handle
 * @param[out] rx_stats - ptr to the rx field in the stats struct
 * @param[out] tx_stats - ptr to the tx field in the stats struct
 *
 */

/*
int vcmGetRtpStats(cc_mcapid_t mcap_id, cc_groupid_t group_id,
                      cc_streamid_t stream_id, cc_call_handle_t call_handle,
                      char *rx_stats, char *tx_stats)
{
    return 0;
}
*/

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

/*
boolean vcmAllocateBandwidth(cc_call_handle_t call_handle, int sessions)
{
    return TRUE;
}
*/

/**
 *
 * Free the bandwidth allocated for this call
 * using the vcmAllocateBandwidth API
 *
 * @note  If not using WLAN provide a stub
 */

/*
void vcmRemoveBandwidth(cc_call_handle_t call_handle)
{
    return;
}
*/

/**
 * @brief vcmActivateWlan
 *
 * Free the bandwidth allocated for this call
 * using the vcmAllocateBandwidth API
 *
 * @note If not using WLAN provide a stub
 */

/*
void vcmActivateWlan(boolean is_active)
{
    return;
}
*/

/**
 *  free the media pointer allocated in vcm_negotiate_attrs method
 *
 *  @param ptr - pointer to be freed
 *
 *  @return  void
 */
void vcmFreeMediaPtr(void *ptr)
{
    return;
}

/**
 *  MEDIA control received from far end on signaling path
 *
 *  @param call_handle - call handle of the call
 *  @param to_encoder - the control request received
 *        Only FAST_PICTURE_UPDATE is supported
 *
 *  @return  void
 *
 */

/*
void vcmMediaControl(cc_call_handle_t call_handle, vcm_media_control_to_encoder_t to_encoder)
{
    return;
}
*/

/**
 *  specifies DSCP marking for RTCP streams
 *
 *  @param group_id - call_id of the call
 *  @param dscp - the DSCP value to be used
 *
 *  @return  void
 */

/*
void vcmSetRtcpDscp(cc_groupid_t group_id, int dscp)
{
    return;
}
*/

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
 * @return boolean - true if attributes are accepted false otherwise
 */

boolean vcmCheckAttribs(uint32_t media_type, void *sdp_p, int level, void **rcapptr)
{
    return TRUE;
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

/*
int vcmDtmfBurst(int digit, int duration, int direction)
{
    return 0;
}
*/

/*
int vcmGetILBCMode()
{
    return SIPSDP_ILBC_MODE20;
}
*/
