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

#ifndef DEVT_H
#define DEVT_H

#ifdef DEBUG
static const char DEVT_CVS_ID[] = "@(#) $RCSfile: devt.h,v $ $Revision: 1.2 $ $Date: 2001/09/18 20:54:28 $ $Name:  $";
#endif /* DEBUG */

/*
 * nssdevt.h
 *
 * This file contains definitions for the low-level cryptoki devices.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

PR_BEGIN_EXTERN_C

/*
 * NSSModule and NSSSlot -- placeholders for the PKCS#11 types
 */

typedef struct NSSModuleStr NSSModule;

typedef struct NSSSlotStr NSSSlot;

typedef struct NSSTokenStr NSSToken;

typedef struct nssSessionStr nssSession;

/* The list of boolean flags used to describe properties of a
 * module.
 */
#define NSSMODULE_FLAGS_NOT_THREADSAFE 0x0001 /* isThreadSafe */

struct NSSModuleStr {
    NSSArena    *arena;
    PRInt32      refCount;
    NSSUTF8     *name;
    NSSUTF8     *libraryPath;
    PRLibrary   *library;
    void        *epv;
    NSSSlot    **slots;
    PRUint32     numSlots;
    PRUint32     flags;
};

/* The list of boolean flags used to describe properties of a
 * slot.
 */
#define NSSSLOT_FLAGS_LOGIN_REQUIRED  0x0001 /* needLogin */
/*#define NSSSLOT_FLAGS_READONLY        0x0002*/ /* readOnly */

struct NSSSlotStr
{
    NSSArena   *arena;
    PRInt32     refCount;
    NSSModule  *module; /* Parent */
    NSSToken   *token;  /* Child (or peer, if you will) */
    NSSUTF8    *name;
    CK_SLOT_ID  slotID;
    void       *epv;
    CK_FLAGS    ckFlags; /* from CK_SLOT_INFO.flags */
    PRUint32    flags;
};

struct NSSTokenStr
{
    NSSArena   *arena;
    PRInt32     refCount;
    NSSSlot    *slot;  /* Parent (or peer, if you will) */
    NSSUTF8    *name;
    CK_FLAGS    ckFlags; /* from CK_TOKEN_INFO.flags */
    PRUint32    flags;
    nssSession *defaultSession;
};

struct nssSessionStr
{
    PZLock *lock;
    CK_SESSION_HANDLE handle;
    NSSSlot *slot;
};

PR_END_EXTERN_C

#endif /* DEVT_H */
