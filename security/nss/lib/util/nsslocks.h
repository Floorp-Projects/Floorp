/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * nsslocks.h - threadsafe functions to initialize lock pointers.
 *
 * NOTE - These are not public interfaces
 *
 * $Id: nsslocks.h,v 1.2 2001/01/03 19:51:06 larryh%netscape.com Exp $
 */

#ifndef _NSSLOCKS_H_
#define _NSSLOCKS_H_

#include "seccomon.h"
#include "nssilock.h"
#include "prmon.h"

SEC_BEGIN_PROTOS

/* Given the address of a (global) pointer to a PZLock, 
 * atomicly create the lock and initialize the (global) pointer, 
 * if it is not already created/initialized.
 */

extern SECStatus nss_InitLock(   PZLock    **ppLock, nssILockType ltype );

/* Given the address of a (global) pointer to a PZMonitor, 
 * atomicly create the monitor and initialize the (global) pointer, 
 * if it is not already created/initialized.
 */

extern SECStatus nss_InitMonitor(PZMonitor **ppMonitor, nssILockType ltype );

SEC_END_PROTOS

#endif
