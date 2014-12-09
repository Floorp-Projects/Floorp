/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_DEBUG_H_
#define _CPR_DEBUG_H_

#include "cpr_types.h"
#include "cpr_stdio.h"

__BEGIN_DECLS

/**
 * CPR debugging flag - MUST be defined at some place in CPR code so that
 * debugging/logging is possible.
 */
extern int32_t cprInfo;

#define CPR_INFO if (cprInfo) notice_msg
#define CPR_ERROR err_msg

__END_DECLS

#endif
