/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PHONE_TYPES_H_
#define _PHONE_TYPES_H_

#include "sessionConstants.h"
#include "cc_constants.h"

#define MAC_ADDRESS_LENGTH 6

typedef cc_lineid_t line_t;
typedef cc_callid_t callid_t;
typedef cc_groupid_t groupid_t;
typedef unsigned short calltype_t;
typedef unsigned short media_refid_t;
typedef cc_streamid_t streamid_t;
typedef cc_mcapid_t mcapid_t;

typedef enum {
    NORMAL_CALL = CC_ATTR_NORMAL,
    XFR_CONSULT = CC_ATTR_XFR_CONSULT,
    CONF_CONSULT = CC_ATTR_CONF_CONSULT,
    ATTR_BARGING = CC_ATTR_BARGING,
    RIUHELD_LOCKED = CC_ATTR_RIUHELD_LOCKED,
    LOCAL_CONF_CONSULT = CC_ATTR_LOCAL_CONF_CONSULT,
    ATTR_MAX
} call_attr_t;

#define CC_GCID_LEN           (129)
#endif
