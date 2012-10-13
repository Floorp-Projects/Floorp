/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_DARWIN_PRIVATE_H_
#define _CPR_DARWIN_PRIVATE_H_

extern pthread_mutexattr_t cprMutexAttributes;

cpr_status_e cprLockInit(void);
cpr_status_e cprTimerInit(void);

#endif
