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

