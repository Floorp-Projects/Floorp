/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_H_
#define _CPR_H_

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_locks.h"
#include "cpr_timers.h"
#include "cpr_threads.h"
#include "cpr_debug.h"
#include "cpr_memory.h"

__BEGIN_DECLS

/*
 * Function prototypes for CPR init routines
 */
/**
 * cprPreInit
 *
 * @brief The cprPreInit function IS called from pSIPCC @b before any components are initialized.
 *
 * This function @b SHOULD initialize those portions of the CPR that
 * are needed before applications can start using it. The memory subsystem
 * (sandbox) is initialized from this routine.
 *
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 * @note pSIPCC will NOT continue and stop initialization if the return value is CPR_FAILURE.
 */
cprRC_t
cprPreInit(void);

/**
 * cprPostInit
 *
 * @brief The cprPostInit function IS called from pSIPCC @b after all the components are initialized.
 *
 * This function @b SHOULD complete any CPR activities before the phone is
 * operational. In other words a call to this function will be the last line of
 * the phone initializtion routine from pSIPCC. This function implementation
 * ties in a couple of debug commands to get more information on the CPR from
 * the shell.
 *
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 * @note pSIPCC will NOT continue and stop initialization if the return value is CPR_FAILURE.
 */
cprRC_t
cprPostInit(void);

__END_DECLS

#endif

