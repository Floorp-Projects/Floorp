/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Definition of error codes.
 *
 * NOTE: When modifying the error codes,
 * also modify the function WebRtcNetEQ_GetErrorCode!
 */

#if !defined NETEQ_ERROR_CODES_H
#define NETEQ_ERROR_CODES_H

/* Misc Error */
#define NETEQ_OTHER_ERROR               -1000

/* Misc Recout Errors */
#define FAULTY_INSTRUCTION              -1001
#define FAULTY_NETWORK_TYPE             -1002
#define FAULTY_DELAYVALUE               -1003
#define FAULTY_PLAYOUTMODE              -1004
#define CORRUPT_INSTANCE                -1005
#define ILLEGAL_MASTER_SLAVE_SWITCH     -1006
#define MASTER_SLAVE_ERROR              -1007

/* Misc Recout problems */
#define UNKNOWN_BUFSTAT_DECISION        -2001
#define RECOUT_ERROR_DECODING           -2002
#define RECOUT_ERROR_SAMPLEUNDERRUN     -2003
#define RECOUT_ERROR_DECODED_TOO_MUCH   -2004

/* Misc RecIn problems */
#define RECIN_CNG_ERROR                 -3001
#define RECIN_UNKNOWNPAYLOAD            -3002
#define RECIN_BUFFERINSERT_ERROR        -3003
#define RECIN_SYNC_RTP_CHANGED_CODEC    -3004
#define RECIN_SYNC_RTP_NOT_ACCEPTABLE   -3005

/* PBUFFER/BUFSTAT ERRORS */
#define PBUFFER_INIT_ERROR              -4001
#define PBUFFER_INSERT_ERROR1           -4002
#define PBUFFER_INSERT_ERROR2           -4003
#define PBUFFER_INSERT_ERROR3           -4004
#define PBUFFER_INSERT_ERROR4           -4005
#define PBUFFER_INSERT_ERROR5           -4006
#define UNKNOWN_G723_HEADER             -4007
#define PBUFFER_NONEXISTING_PACKET      -4008
#define PBUFFER_NOT_INITIALIZED         -4009
#define AMBIGUOUS_ILBC_FRAME_SIZE       -4010

/* CODEC DATABASE ERRORS */
#define CODEC_DB_FULL                   -5001
#define CODEC_DB_NOT_EXIST1             -5002
#define CODEC_DB_NOT_EXIST2             -5003
#define CODEC_DB_NOT_EXIST3             -5004
#define CODEC_DB_NOT_EXIST4             -5005
#define CODEC_DB_UNKNOWN_CODEC          -5006
#define CODEC_DB_PAYLOAD_TAKEN          -5007
#define CODEC_DB_UNSUPPORTED_CODEC      -5008
#define CODEC_DB_UNSUPPORTED_FS         -5009

/* DTMF ERRORS */
#define DTMF_DEC_PARAMETER_ERROR        -6001
#define DTMF_INSERT_ERROR               -6002
#define DTMF_GEN_UNKNOWN_SAMP_FREQ      -6003
#define DTMF_NOT_SUPPORTED              -6004

/* RTP/PACKET ERRORS */
#define RED_SPLIT_ERROR1                -7001
#define RED_SPLIT_ERROR2                -7002
#define RTP_TOO_SHORT_PACKET            -7003
#define RTP_CORRUPT_PACKET              -7004

#endif
