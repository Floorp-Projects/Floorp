/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debug.h"
#include "rtp_defs.h"
#include "vcm_util.h"
#include "ccsdp.h"

#define CREATE_MT_MAP(a,b)  ((a << 16) | b)

/**
 * map the VCM RTP enums to MS format payload enums
 * @param [in] ptype: payload type
 * @param [in] dynamic_ptype_value : dynamic payload
 * @param [in] mode for some codecs
 * @return Corresponding MS payload type
 */
vcm_media_payload_type_t vcmRtpToMediaPayload (int32_t ptype,
                                            int32_t dynamic_ptype_value,
                                            uint16_t mode)
{
    vcm_media_payload_type_t type = VCM_Media_Payload_G711Ulaw64k;
    const char fname[] = "vcm_rtp_to_media_payload";

    DEF_DEBUG(DEB_F_PREFIX"%d: %d: %d\n",
              DEB_F_PREFIX_ARGS(MED_API, fname), ptype, dynamic_ptype_value,
              mode);
    switch (ptype) {
    case RTP_PCMU:
        type = VCM_Media_Payload_G711Ulaw64k;
        break;

    case RTP_PCMA:
        type = VCM_Media_Payload_G711Alaw64k;
        break;

    case RTP_G729:
        type = VCM_Media_Payload_G729;
        break;

    case RTP_L16:
        type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_Wide_Band_256k);
        break;

    case RTP_G722:
        type = VCM_Media_Payload_G722_64k;
        break;

    case RTP_ILBC:
        if (mode == SIPSDP_ILBC_MODE20) {
            type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_ILBC20);
        } else {
            type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_ILBC30);
        }
        break;

    case RTP_ISAC:
        type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_ISAC);
        break;

    case RTP_H263:
        type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_H263);
        break;

    case RTP_H264_P0:
    case RTP_H264_P1:
        type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_H264);
        break;
    case RTP_VP8:
        type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_VP8);
        break;

    case RTP_OPUS:
        type = CREATE_MT_MAP(dynamic_ptype_value, VCM_Media_Payload_OPUS);
        break;

    default:
        type = VCM_Media_Payload_NonStandard;
        err_msg(DEB_F_PREFIX "Can not map payload type=%d\n",
                  DEB_F_PREFIX_ARGS(MED_API, fname), ptype);
    }
    return type;
}
