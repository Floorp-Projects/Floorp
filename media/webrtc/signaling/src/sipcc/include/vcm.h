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

/** @section vcm  VCM APIs
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

#ifndef _VCM_H_
#define _VCM_H_

#include "cpr_types.h"
#include "cc_constants.h"

/**
 * Macro definitions to retrieve dynamic payload type and media payload type from vcm_media_payload_type_t payload
 * for using in vcmRx/TxStart.
 */
//The result maps to RTP payload type for using internally.
#define GET_DYNAMIC_PAY_LOAD_TYPE(payload) (payload>>16)
//The result maps to the enum vcm_media_payload_type_t. 
#define GET_MEDIA_PAYLOAD_TYPE_ENUM(payload) (payload & 0xFFFF)

/** Evaluates to TRUE for audio media streams where id is the mcap_id of the given stream */
#define CC_IS_AUDIO(id) ((id == CC_AUDIO_1) ? TRUE:FALSE)
/** Evaluates to TRUE for video media streams where id is the mcap_id of the given stream */
#define CC_IS_VIDEO(id) ((id == CC_VIDEO_1) ? TRUE:FALSE)


/** Definitions for direction requesting Play tone to user */
#define VCM_PLAY_TONE_TO_EAR      1
/** Definitions value for direction requesting Play tone to network stream or far end */
#define VCM_PLAY_TONE_TO_NET      2
/** Definitions value for direction requesting Play tone to both user and network */
#define VCM_PLAY_TONE_TO_ALL      3

/** Definitions for alert_info in vcmToneStartWithSpeakerAsBackup API */
#define VCM_ALERT_INFO_OFF        0
/** Definitions for alert_info in vcmToneStartWithSpeakerAsBackup API */
#define VCM_ALERT_INFO_ON         1

/** Definitions for DSP Codec Resources. */
#define VCM_CODEC_RESOURCE_G711     0x00000001
#define VCM_CODEC_RESOURCE_G729A    0x00000002
#define VCM_CODEC_RESOURCE_G729B    0x00000004
#define VCM_CODEC_RESOURCE_LINEAR   0x00000008
#define VCM_CODEC_RESOURCE_G722     0x00000010
#define VCM_CODEC_RESOURCE_iLBC     0x00000020
#define VCM_CODEC_RESOURCE_iSAC     0x00000040
#define VCM_CODEC_RESOURCE_H264     0x00000080
#define VCM_CODEC_RESOURCE_H263     0x00000002
#define VCM_CODEC_RESOURCE_VP8      0x00000100
#define VCM_CODEC_RESOURCE_I420     0x00000200

#define VCM_DSP_DECODEONLY  0
#define VCM_DSP_ENCODEONLY  1
#define VCM_DSP_FULLDUPLEX  2
#define VCM_DSP_IGNORE      3

#define CC_KFACTOR_STAT_LEN   (256)


/**
 *  vcm_tones_t
 *  Enum identifying various tones that the media layer should implement
 */

typedef enum
{
    VCM_INSIDE_DIAL_TONE,
    VCM_OUTSIDE_DIAL_TONE,
    VCM_DEFAULT_TONE = 1,
    VCM_LINE_BUSY_TONE,
    VCM_ALERTING_TONE,
    VCM_BUSY_VERIFY_TONE,
    VCM_STUTTER_TONE,
    VCM_MSG_WAITING_TONE,
    VCM_REORDER_TONE,
    VCM_CALL_WAITING_TONE,
    VCM_CALL_WAITING_2_TONE,
    VCM_CALL_WAITING_3_TONE,
    VCM_CALL_WAITING_4_TONE,
    VCM_HOLD_TONE,
    VCM_CONFIRMATION_TONE,
    VCM_PERMANENT_SIGNAL_TONE,
    VCM_REMINDER_RING_TONE,
    VCM_NO_TONE,
    VCM_ZIP_ZIP,
    VCM_ZIP,
    VCM_BEEP_BONK,
/*#$#$#$#$#@$#$#$#$#$#$#$#$#$#$#$#$$#$#$#$#$#$#$#$#$
 * There is a corresponding table defined in
 * dialplan.c tone_names[]. Make sure to add tone
 * name in that table if you add any new entry above
 */
    VCM_RECORDERWARNING_TONE,
    VCM_RECORDERDETECTED_TONE,
    VCM_MONITORWARNING_TONE,
    VCM_SECUREWARNING_TONE,
    VCM_NONSECUREWARNING_TONE,
    VCM_MAX_TONE,
    VCM_MAX_DIALTONE = VCM_BEEP_BONK
} vcm_tones_t;


/**
 *  vcm_tones_t
 *  Enum identifying various tones that the media layer should implement
 */


/**
 * vcm_ring_mode_t
 * VCM_RING_OFFSET is used to map the list
 * of ring names to the correct enum type
 * when parsing the alert-info header.
 */
typedef enum
{
    VCM_RING_OFF     = 0x1,
    VCM_INSIDE_RING  = 0x2,
    VCM_OUTSIDE_RING = 0x3,
    VCM_FEATURE_RING = 0x4,
    VCM_BELLCORE_DR1 = 0x5,
    VCM_RING_OFFSET  = 0x5,
    VCM_BELLCORE_DR2 = 0x6,
    VCM_BELLCORE_DR3 = 0x7,
    VCM_BELLCORE_DR4 = 0x8,
    VCM_BELLCORE_DR5 = 0x9,
    VCM_BELLCORE_MAX = VCM_BELLCORE_DR5,
    VCM_FLASHONLY_RING = 0xA,
    VCM_STATION_PRECEDENCE_RING = 0xB,
    VCM_MAX_RING = 0xC
} vcm_ring_mode_t;

/**
 * vcm_ring_duration_t
 * Enums for specifying normal vs single ring
 */
typedef enum {
    vcm_station_normal_ring = 0x1,
    vcm_station_single_ring = 0x2
} vcm_ring_duration_t;


/**
 * Media payload type definitions
 */
typedef enum
{
    VCM_Media_Payload_NonStandard = 1,
    VCM_Media_Payload_G711Alaw64k = 2,
    VCM_Media_Payload_G711Alaw56k = 3, // "restricted"
    VCM_Media_Payload_G711Ulaw64k = 4,
    VCM_Media_Payload_G711Ulaw56k = 5, // "restricted"
    VCM_Media_Payload_G722_64k = 6,
    VCM_Media_Payload_G722_56k = 7,
    VCM_Media_Payload_G722_48k = 8,
    VCM_Media_Payload_G7231 = 9,
    VCM_Media_Payload_G728 = 10,
    VCM_Media_Payload_G729 = 11,
    VCM_Media_Payload_G729AnnexA = 12,
    VCM_Media_Payload_Is11172AudioCap = 13,
    VCM_Media_Payload_Is13818AudioCap = 14,
    VCM_Media_Payload_G729AnnexB = 15,
    VCM_Media_Payload_G729AnnexAwAnnexB = 16,
    VCM_Media_Payload_GSM_Full_Rate = 18,
    VCM_Media_Payload_GSM_Half_Rate = 19,
    VCM_Media_Payload_GSM_Enhanced_Full_Rate = 20,
    VCM_Media_Payload_Wide_Band_256k = 25,
    VCM_Media_Payload_H263 = 31,
    VCM_Media_Payload_H264 = 34,
    VCM_Media_Payload_Data64 = 32,
    VCM_Media_Payload_Data56 = 33,
    VCM_Media_Payload_ILBC20 = 39,
    VCM_Media_Payload_ILBC30 = 40,
    VCM_Media_Payload_ISAC = 41,
    VCM_Media_Payload_GSM = 80,
    VCM_Media_Payload_ActiveVoice = 81,
    VCM_Media_Payload_G726_32K = 82,
    VCM_Media_Payload_G726_24K = 83,
    VCM_Media_Payload_G726_16K = 84,
    VCM_Media_Payload_VP8 = 120,
    VCM_Media_Payload_I420 = 124,
    VCM_Media_Payload_Max           // Please leave this so we won't get compile errors.
} vcm_media_payload_type_t;

/**
 * vcm_vad_t
 * Enums for Voice Activity Detection
 */
typedef enum vcm_vad_t_ {
    VCM_VAD_OFF = 0,
    VCM_VAD_ON = 1
} vcm_vad_t;

/**
 * vcm_audio_bits_t
 * Enums for indicating audio path
 */
typedef enum vcm_audio_bits_ {
    VCM_AUDIO_NONE,
    VCM_AUDIO_HANDSET,
    VCM_AUDIO_HEADSET,
    VCM_AUDIO_SPEAKER
} vcm_audio_bits_t;

/**
 * vcm_crypto_algorithmID
 * Crypto parameters for SRTP media
 */
typedef enum {
    VCM_INVLID_ALGORITM_ID = -1,    /* invalid algorithm ID.             */
    VCM_NO_ENCRYPTION = 0,          /* no encryption                     */
    VCM_AES_128_COUNTER             /* AES 128 counter mode (32 bits HMAC)*/
} vcm_crypto_algorithmID;

/**
 * vcm_mixing_mode_t
 * Mixing mode for media
 */
typedef enum {
    VCM_NO_MIX,
    VCM_MIX
} vcm_mixing_mode_t;

/**
 * vcm_session_t
 * Media Session Specification enum
 */
typedef enum {
    PRIMARY_SESSION,
    MIX_SESSION,
    NO_SESSION
} vcm_session_t;


/**
 * vcm_mixing_party_t
 * Media mix party enum
 * Indicates the party to be mixed none/local/remote/both/TxBOTH_RxNONE
 * TxBOTH_RxNONE means that for Tx stream it's both, for Rx stream it's none
 */
typedef enum {
    VCM_PARTY_NONE,
    VCM_PARTY_LOCAL,
    VCM_PARTY_REMOTE,
    VCM_PARTY_BOTH,
    VCM_PARTY_TxBOTH_RxNONE
} vcm_mixing_party_t;

/**
 * media_control_to_encoder_t
 * Enums for far end control for media
 * Only Fast Picture Update for video is supported
 */
typedef enum {
    VCM_MEDIA_CONTROL_PICTURE_FAST_UPDATE
} vcm_media_control_to_encoder_t;

/**
 * Maximum key and salt must be adjust to the largest possible
 * supported key.
 */
#define VCM_SRTP_MAX_KEY_SIZE  16   /* maximum key in bytes (128 bits) */
/**
 * Maximum key and salt must be adjust to the largest possible
 * supported salt.
 */
#define VCM_SRTP_MAX_SALT_SIZE 14   /* maximum salt in bytes (112 bits)*/

/** Key size in bytes for AES128 algorithm */
#define VCM_AES_128_COUNTER_KEY_SIZE  16
/** Salt size in bytes for AES128 algorithm */
#define VCM_AES_128_COUNTER_SALT_SIZE 14

/** Structure to carry crypto key and salt for SRTP streams */
typedef struct vcm_crypto_key_t_ {
    cc_uint8_t key_len; /**< key length*/
    cc_uint8_t key[VCM_SRTP_MAX_KEY_SIZE]; /**< key*/
    cc_uint8_t salt_len; /**< salt length*/
    cc_uint8_t salt[VCM_SRTP_MAX_SALT_SIZE]; /**< salt*/
} vcm_crypto_key_t;

/**
 *  vcm_videoAttrs_t
 *  An opaque handle to store and pass video attributes
 */
typedef struct vcm_videoAttrs_t_ {
  void * opaque; /**< pointer to opaque data from application as received from vcm_negotiate_attrs()*/
} vcm_videoAttrs_t;

/**  A structure carrying audio media specific attributes */
typedef struct vcm_audioAttrs_t_ {
  cc_uint16_t packetization_period; /**< ptime value received in SDP */
  cc_int32_t avt_payload_type; /**< RTP payload type for AVT */
  vcm_vad_t vad; /**< Voice Activity Detection on or off */
  vcm_mixing_party_t mixing_party; /**< mixing_party */
  vcm_mixing_mode_t  mixing_mode;  /**< mixing_mode*/
} vcm_audioAttrs_t;


/**
 *  vcm_mediaAttrs_t
 *  A structure to contain audio or video media attributes
 */
typedef struct vcm_attrs_t_ {
  cc_boolean         mute;
  vcm_audioAttrs_t audio; /**< audio line attribs */
  vcm_videoAttrs_t video; /**< Video Atrribs */
} vcm_mediaAttrs_t;

//Using C++ for gips. This is required for gips.
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  The initialization of the VCM module
 *
 */

void vcmInit();

/**
 * The unloading of the VCM module
 */
void vcmUnload();


/**
 *   Should we remove this from external API
 *
 *  @param[in] mcap_id - group identifier to which stream belongs.
 *  @param[in]     group_id         - group identifier
 *  @param[in]     stream_id        - stream identifier
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

short vcmRxOpen(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t call_handle,
        cc_uint16_t port_requested,
        cpr_ip_addr_t *listen_ip,
        cc_boolean is_multicast,
        int *port_allocated);
/*!
 *  should we remove from external API
 *
 *  @param[in]  mcap_id - Media Capability ID
 *  @param[in]  group_id - group to which stream belongs
 *  @param[in]  stream_id - stream identifier
 *  @param[in]  call_handle - call handle
 *
 *  @return  zero(0) for success; otherwise, ERROR for failure
 *
 */

short vcmTxOpen(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t call_handle);

/*!
 *  Allocate(Reserve) a receive port.
 *
 *  @param[in]  mcap_id - Media Capability ID
 *  @param[in]  group_id - group identifier to which stream belongs.
 *  @param[in]  stream_id - stream identifier
 *  @param[in]  call_handle  - call handle
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
        int *port_allocated);

/*!
 *  Release the allocated port
 * @param[in] mcap_id   - media capability id (0 is audio)
 * @param[in] group_id  - group identifier
 * @param[in] stream_id - stream identifier
 * @param[in] call_handle   - call handle
 * @param[in] port     - port to be released
 *
 * @return void
 */

void vcmRxReleasePort(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle,
        int port);

/*!
 *  Start receive stream
 *  Note: For video calls, for a given call handle there will be
 *        two media lines and the corresponding group_id/stream_id pair.
 *        One RTP session is requested from media server for each
 *        media line(group/stream) i.e. a video call would result in
 *        two rtp_sessions in our session info list created by two
 *        calls to vcm_rx/tx with mcap_id of AUDIO and VIDEO respectively.
 *
 *  @param[in]    mcap_id     - media type id
 *  @param[in]    group_id    - group identifier associated with the stream
 *  @param[in]    stream_id   - id of the stream one per each media line
 *  @param[in]    call_handle - call handle
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
        vcm_mediaAttrs_t *attrs);

/**
 *  start tx stream
 *  Note: For video calls, for a given call handle there will be
 *        two media lines and the corresponding group_id/stream_id pair.
 *        One RTP session is requested from media server for each
 *        media line(group/stream) i.e. a video call would result in
 *        two rtp_sessions in our session info list created by two
 *        calls to vcm_rx/tx with mcap_id of AUDIO and VIDEO respectively.
 *
 *  @param[in]   mcap_id      - media cap id
 *  @param[in]   group_id     - group identifier to which the stream belongs
 *  @param[in]   stream_id    - stream id of the given media type.
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
        vcm_mediaAttrs_t *attrs);

/*!
 *  Close the receive stream.
 *
 *  @param[in]    mcap_id - Media Capability ID
 *  @param[in]    group_id - group identifier that belongs to the stream.
 *  @param[in]    stream_id - stream id of the given media type.
 *  @param[in]    call_handle - call handle
 *
 *  @return   None
 *
 */

void vcmRxClose(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle);

/**
 *  Close the transmit stream
 *
 *  @param[in] mcap_id - Media Capability ID
 *  @param[in] group_id - identifier of the group to which stream belongs
 *  @param[in] stream_id - stream id of the given media type.
 *  @param[in] call_handle - call handle
 *
 *  @return     void
 */

void vcmTxClose(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t  call_handle);

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
void vcmEnableSidetone(cc_uint16_t side_tone);

/**
 *  Start a tone (continuous)
 *
 *  Parameters:
 *  @param[in] tone       - tone type
 *  @param[in] alert_info - alertinfo header
 *  @param[in] call_handle- call handle
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
        cc_uint16_t direction);

/**
 * Plays a short tone. uses the open audio path.
 * If no audio path is open, plays on speaker.
 *
 * @param[in] tone       - tone type
 * @param[in] alert_info - alertinfo header
 * @param[in] call_handle- call handle
 * @param[in] group_id - identifier of the group to which stream belongs
 * @param[in] stream_id   - stream identifier.
 * @param[in] direction  - network, speaker, both
 *
 * @return void
 */

void vcmToneStartWithSpeakerAsBackup(vcm_tones_t tone,
        short alert_info,
        cc_call_handle_t call_handle,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_uint16_t direction);

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
        cc_call_handle_t call_handle);

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

void vcmControlRinger(vcm_ring_mode_t ringMode,
        short once,
        cc_boolean alert_info,
        int line,
        cc_callid_t call_id);


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

int vcmGetAudioCodecList(int request_type);

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

int vcmGetVideoCodecList(int request_type);

/**
 * Get max supported H.264 video packetization mode. 
 * @return maximum supported video packetization mode for H.264. Value returned
 * must be 0 or 1. Value 2 is not supported yet.
 */
int vcmGetVideoMaxSupportedPacketizationMode();

/**
 * Get the rx/tx stream statistics associated with the call.
 * The rx/tx stats are defined as comma seperated string as follows.
 * Rx_stats: 
 *   snprintf(rx_stats, CC_KFACTOR_STAT_LEN,
 *               "Dur=%d,Pkt=%d,Oct=%d,LatePkt=%d,LostPkt=%d,AvgJit=%d,VQMetrics=\"%s\"",
 *               duration, numberOfPackageReceived, numberOfByteReceived, numberOfLatePackage, numberOfPackageLost, averageJitter, qualityMatrics);
 * Tx_stats:
 *   snprintf(tx_stats, CC_KFACTOR_STAT_LEN, "Dur=%d,Pkt=%d,Oct=%d",
 *               duration, numberOfPackageSent, numberOfByteSend);
 *
 *Example:
 *
 *   vcm_rtp_get_stats : rx_stats:Dur=1,Pkt=90,Oct=15480,LatePkt=0,LostPkt=0,AvgJit=0,VQMetrics="MLQK=0.0;MLQKav=0.0;MLQKmn=0.0;MLQKmx=0.0;MLQKvr=0.95;CCR=0.0;ICR=0.0;ICRmx=0.0;CS=0;SCS=0"
 *   vcm_rtp_get_stats : tx_stats:Dur=1,Pkt=92,Oct=14720
 *   Where the duration can be calculated, the numberOfLatePackage set to 0.
 *
 * @param[in]  mcap_id  - media type (audio/video)
 * @param[in]  group_id - group id of the stream
 * @param[in]  stream_id - stram id of the stream
 * @param[in]  call_handle - call handle
 * @param[out] rx_stats - ptr to the rx field in the stats struct, see above.
 * @param[out] tx_stats - ptr to the tx field in the stats struct, see above.
 *
 */

int vcmGetRtpStats(cc_mcapid_t mcap_id,
        cc_groupid_t group_id,
        cc_streamid_t stream_id,
        cc_call_handle_t call_handle,
        char *rx_stats,
        char *tx_stats);

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

cc_boolean vcmAllocateBandwidth(cc_call_handle_t call_handle, int sessions);

/**
 *
 * Free the bandwidth allocated for this call
 * using the vcmAllocateBandwidth API
 *
 * @note  If not using WLAN provide a stub
 */

void vcmRemoveBandwidth(cc_call_handle_t call_handle);

/**
 * @brief vcmActivateWlan
 *
 * Free the bandwidth allocated for this call
 * using the vcmAllocateBandwidth API
 *
 * @note If not using WLAN provide a stub
 */

void vcmActivateWlan(cc_boolean is_active);

/**
 *  free the media pointer allocated in vcm_negotiate_attrs method
 *
 *  @param ptr - pointer to be freed
 *
 *  @return  void
 */
void vcmFreeMediaPtr(void *ptr);

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

void vcmMediaControl(cc_call_handle_t call_handle, vcm_media_control_to_encoder_t to_encoder);

/**
 *  specifies DSCP marking for RTCP streams
 *
 *  @param group_id - group id of the stream
 *  @param dscp - the DSCP value to be used
 *
 *  @return  void
 */

void vcmSetRtcpDscp(cc_groupid_t group_id, int dscp);

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

cc_boolean vcmCheckAttribs(cc_uint32_t media_type, void *sdp_p, int level, void **rcapptr);

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
                          cc_uint16_t payload_number, cc_boolean isOffer);


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
int vcmDtmfBurst(int digit, int duration, int direction);

/**
 * vcmGetILBCMode
 *
 * This method should return the mode that needs to be used in 
 * SDP
 * @return int
 */
int vcmGetILBCMode(); 


//Using C++ for gips. This is the end of extern "C" above.
#ifdef __cplusplus
}
#endif


#endif /* _VCM_H_ */
