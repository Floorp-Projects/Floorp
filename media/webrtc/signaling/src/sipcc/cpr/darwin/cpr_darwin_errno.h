/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_DARWIN_ERRNO_H_
#define _CPR_DARWIN_ERRNO_H_

#include <errno.h>

/*
 * Maintain re-entrant nature by wrapping 'errno'
 */
/** @def cpr_errno is used by pSIPCC. MUST be defined by the CPR layer.
  */
#define cpr_errno cprTranslateErrno()

int16_t cprTranslateErrno(void);

#endif
