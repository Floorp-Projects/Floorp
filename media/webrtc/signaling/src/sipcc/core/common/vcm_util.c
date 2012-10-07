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
