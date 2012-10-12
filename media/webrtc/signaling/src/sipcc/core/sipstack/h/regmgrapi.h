/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _REGMGRAPI_H_
#define _REGMGRAPI_H_

#include "sessionConstants.h"

typedef enum reg_mode_t_ {
    REG_MODE_CCM = CC_MODE_CCM,
    REG_MODE_NON_CCM = CC_MODE_NONCCM,
} reg_mode_t;

reg_mode_t sip_regmgr_get_cc_mode(line_t line);

#endif /* _REGMGRAPI_H_ */
