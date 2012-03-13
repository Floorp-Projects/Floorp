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

#ifndef RTP_DEFS_H
#define RTP_DEFS_H

#include "cpr_types.h"

#define SAMPLES_TO_MILLISECONDS(SAMPLES) ((SAMPLES)>>3)
#define MILLISECONDS_TO_SAMPLES(PERIOD)  ((PERIOD)<<3)

#define MAX_TX_RTP_PORTS             2
#define MAX_FRAMES_PER_PACKET        6
#define MAX_VOICE_FRAME_SIZE       320

#define GSM_EFR_FRAME_SIZE          32
#define GSM_FR_FRAME_SIZE           33
#define G729_FRAME_SIZE             10
#define G723_FRAME_SIZE63           24
#define G723_FRAME_SIZE53           20
#define G723_SID_FRAME_SIZE          4
#define G729_SID_FRAME_SIZE          2

#define GSM_SAMPLES_PER_FRAME      160
#define G729_SAMPLES_PER_FRAME      80
#define G723_SAMPLES_PER_FRAME     240
#define LINEAR_16KHZ_SAMPLES_PER_FRAME  160  // 10 ms = 160 samples @ 16 kHz

#define MAX_ARM_TO_DSP_CHANNEL  3
#define MAX_DSP_TO_ARM_CHANNEL  2
#define HALF_SIZE_DATA_INGRESS  240
#define RX_MAX MAX_ARM_TO_DSP_CHANNEL

#define OPEN_OK                      0
#define OPEN_ERROR_DUPLICATE        -1

#define ASSIGN_TX_CHANNEL           (0x1)
#define ASSIGN_RX_CHANNEL           (0x2)
#define CHANNEL_CLOSE_IN_PROGRESS   (0x80000000)

#define RTP_START_PORT              0x4000
#define RTP_END_PORT                0x7FFE

#define GET_DYN_PAYLOAD_TYPE_VALUE(a) ((a & 0XFF00) ? ((a & 0XFF00) >> 8) : a)
#define SET_PAYLOAD_TYPE_WITH_DYNAMIC(a,b) ((a << 8) | b)


//=============================================================================
//
//  Enumeration Types
//
//-----------------------------------------------------------------------------
enum RTP_PAYLOAD_TYPES
{
    G711_MULAW_PAYLOAD_TYPE         = 0,
    GSM_FR_PAYLOAD_TYPE             = 3,
    G723_PAYLOAD_TYPE               = 4,
    G711_ALAW_PAYLOAD_TYPE          = 8,
    LINEAR_8KHZ_PAYLOAD_TYPE        = 12,
    TYPE13_SID_PAYLOAD_TYPE         = 13,
    G729_PAYLOAD_TYPE               = 18,
    GSM_EFR_PAYLOAD_TYPE            = 20,
    LINEAR_16KHZ_PAYLOAD_TYPE       = 25,
    AVT_PAYLOAD_TYPE                = 101,
    MASK_PAYLOAD_TYPE               = 0x7f
};

enum RTP_TRANSMIT_STATES
{
    RTP_TX_FRAME,
    RTP_TX_START,
    RTP_TX_NO_FRAME,
    RTP_TX_END = RTP_TX_NO_FRAME,
    RTP_TX_SID
};

enum RTP_RX_STATES
{
    RTP_RX_NORMAL,
    RTP_RX_FLUSH_SOON,
    RTP_RX_FLUSH_NOW
};

enum RTP_TALKERS_TYPES
{
    FIRST_TALKER = 0,
    LAST_TALKER = RX_MAX - 1,
    NO_TALKER
};

typedef enum
{
    RTP_INGRESS = 0,
    RTP_EGRESS
} t_RtpDirection;

//=============================================================================
//
//  Structure/Type definitions
//
//-----------------------------------------------------------------------------
typedef uint16_t rtp_channel_t;

/********************************/
/* RTP Call Stats Descriptor    */
/*                              */
/********************************/

typedef struct
{
    int call_id;
    unsigned long Rxduration;
    unsigned long Rxpackets;
    unsigned long Rxoctets;
    unsigned long Rxlatepkts;
    unsigned long Rxlostpkts;
    unsigned long Txduration;
    unsigned long Txpackets;
    unsigned long Txoctets;
} t_callstats;

#endif
