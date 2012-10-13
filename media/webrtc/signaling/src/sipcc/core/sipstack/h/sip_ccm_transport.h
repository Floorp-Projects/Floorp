/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SIP_CCM_TRANSPORT_H__
#define __SIP_CCM_TRANSPORT_H__

#include "cpr_types.h"
#include "cpr_socket.h"
#include "phone_types.h"

#define CCM_ID_PRINT(arg) \
        (arg == PRIMARY_CCM ?  "PRIMARY_CCM" : \
        arg == SECONDARY_CCM ?  "SECONDARY_CCM" : \
        arg == TERTIARY_CCM ?  "TERTIARY_CCM" : \
        arg == MAX_CCM ?  "MAX_CCM" : \
        arg == UNUSED_PARAM ?  "UNUSED_PARAM" : "Unknown")\


#endif /* __SIP_CCM_TRANSPORT_H__ */
