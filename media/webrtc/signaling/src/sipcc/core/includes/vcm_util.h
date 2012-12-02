/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VCM_UTIL_H_
#define VCM_UTIL_H_
#include "vcm.h"
#include "phone_types.h"

/**
 * map the VCM RTP enums to MS format payload enums
 * @param [in] ptype: payload type
 * @param [in] dynamic_ptype_value : dynamic payload
 * @param [in] mode for some codecs
 * @return Corresponding MS payload type
 */
vcm_media_payload_type_t vcm_rtp_to_media_payload (int32_t ptype,
                                            int32_t dynamic_ptype_value,
                                            uint16_t mode);


#endif /* VCM_UTIL_H_ */
