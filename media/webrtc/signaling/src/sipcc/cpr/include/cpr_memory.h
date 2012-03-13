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

#ifndef _CPR_MEMORY_H_
#define _CPR_MEMORY_H_

#include "cpr_types.h"

__BEGIN_DECLS

/**
 * Define handles for buffer region, pools and actual buffers
 */
typedef void *cprBuffer_t;

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_memory.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_memory.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_memory.h"
#endif

/**
 * @brief Retrieve a buffer. 
 *
 * The cprGetBuffer function retrieves a single buffer from the heap. 
 * The buffer is guaranteed to be at least the size passed in, but it may be
 * larger. CPR does not clean the buffers when they are returned so it is highly
 * recommended that the application set all values in the buffer to some default
 * values before using it.
 *
 * @param[in] size          requested size of the buffer in bytes
 *
 * @return              Handle to a buffer or NULL if the get failed
 *
 * @note                The buffer may be larger than the size requested.
 * @note                The buffer will come from the system's heap
 */
#if defined SIP_OS_WINDOWS
#define cprGetBuffer(y)  cpr_calloc(1,y)
#else
void *cprGetBuffer(uint32_t size);
#endif

/**
 * cprReleaseBuffer
 *
 * @brief Returns a buffer to the heap.
 * CPR keeps track of this information so the application only
 * needs to pass a pointer to the buffer to return.
 *
 * @param[in] bufferPtr  pointer to the buffer to return
 *
 * @return        none
 */
void cprReleaseBuffer(void *bufferPtr);

/**
 * @brief Given a msg buffer, returns a pointer to the buffer's header
 *
 * The cprGetSysHeader function retrieves the system header buffer for the
 * passed in message buffer.
 *
 * @param[in] bufferPtr  pointer to the buffer whose sysHdr to return
 *
 * @return        Abstract pointer to the msg buffer's system header
 *                or #NULL if failure
 */
void *cprGetSysHeader(void *bufferPtr);

/**
 * @brief Called when the application is done with this system header
 *
 * The cprReleaseSysHeader function returns the system header buffer to the
 * system.
 * @param[in] sysHdrPtr  pointer to the sysHdr to be released
 *
 * @return        none
 */
void cprReleaseSysHeader(void *sysHdrPtr);

__END_DECLS

#endif
