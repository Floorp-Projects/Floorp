/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* XPTI_PUBLIC_API and XPTI_GetInterfaceInfoManager declarations. */

#ifndef xptcall_h___
#define xptcall_h___

#include "prtypes.h"
#include "nscore.h"
#include "nsISupports.h"
#include "xpt_struct.h"
#include "xpt_cpp.h"
#include "xptinfo.h"
#include "nsIInterfaceInfo.h"

/***************************************************************************/
/*
 * The linkage of XPTC API functions differs depending on whether the file is
 * used within the XPTC library or not.  Any source file within the XPTC
 * library should define EXPORT_XPTC_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_XPTC_API
#define XPTC_PUBLIC_API(t)    PR_IMPLEMENT(t)
#define XPTC_PUBLIC_DATA(t)   PR_IMPLEMENT_DATA(t)
#ifdef _WIN32
#    define XPTC_EXPORT           _declspec(dllexport)
#else
#    define XPTC_EXPORT
#endif
#else
#ifdef _WIN32
#    define XPTC_PUBLIC_API(t)    _declspec(dllimport) t
#    define XPTC_PUBLIC_DATA(t)   _declspec(dllimport) t
#    define XPTC_EXPORT           _declspec(dllimport)
#else
#    define XPTC_PUBLIC_API(t)    PR_IMPLEMENT(t)
#    define XPTC_PUBLIC_DATA(t)   t
#    define XPTC_EXPORT
#endif
#endif
#define XPTC_FRIEND_API(t)    XPTC_PUBLIC_API(t)
#define XPTC_FRIEND_DATA(t)   XPTC_PUBLIC_DATA(t)
/***************************************************************************/

struct nsXPTCMiniVariant
{
// No ctors or dtors so that we can use arrays of these on the stack
// with no penalty.
    union
    {
        PRInt8   i8;
        PRInt16  i16;
        PRInt32  i32;
        PRInt64  i64;
        PRUint8  u8;
        PRUint16 u16;
        PRUint32 u32;
        PRUint64 u64;
        float    f;
        double   d;
        PRBool   b;
        char     c;
        wchar_t  wc;
        void*    p;
    } val;
};

struct nsXPTCVariant : public nsXPTCMiniVariant
{
// No ctors or dtors so that we can use arrays of these on the stack
// with no penalty.

    // inherits 'val' here
    void*     ptr;
    nsXPTType type;
    uint8     flags;

    enum
    {
        // these are bitflags!
        PTR_IS_DATA  = 0x1, // ptr points to 'real' data in val
        VAL_IS_OWNED = 0x2, // val.p holds alloc'd ptr that must be freed
        VAL_IS_IFACE = 0x4  // val.p holds interface ptr that must be released
    };

    PRBool IsPtrData()      const {return (PRBool) (flags & PTR_IS_DATA);}
    PRBool IsValOwned()     const {return (PRBool) (flags & VAL_IS_OWNED);}
    PRBool IsValInterface() const {return (PRBool) (flags & VAL_IS_IFACE);}
};

/***************************************************************************/

class nsXPTCStubBase : public nsISupports
{
public:
    // Include generated vtbl stub declarations.
    // These are virtual and *also* implemented by this class..
#include "xptcstubsdecl.inc"

    // The following methods must be provided by inheritor of this class.

    // return a refcounted pointer to the InterfaceInfo for this object
    // NOTE: on some platforms this MUST not fail or we crash!
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info) = 0;

    // call this method and return result
    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params) = 0;
};


PR_BEGIN_EXTERN_C

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);

PR_END_EXTERN_C

#endif /* xptcall_h___ */
