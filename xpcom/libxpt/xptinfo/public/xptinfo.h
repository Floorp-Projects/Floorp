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

#ifndef xptiinfo_h___
#define xptiinfo_h___

#include "prtypes.h"

/*
 * The linkage of XPTI API functions differs depending on whether the file is
 * used within the XPTI library or not.  Any source file within the XPTI
 * library should define EXPORT_XPTI_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_XPTI_API
#define XPTI_PUBLIC_API(t)    PR_IMPLEMENT(t)
#define XPTI_PUBLIC_DATA(t)   PR_IMPLEMENT_DATA(t)
#ifdef _WIN32
#    define XPTI_EXPORT           _declspec(dllexport)
#else
#    define XPTI_EXPORT
#endif
#else
#ifdef _WIN32
#    define XPTI_PUBLIC_API(t)    _declspec(dllimport) t
#    define XPTI_PUBLIC_DATA(t)   _declspec(dllimport) t
#    define XPTI_EXPORT           _declspec(dllimport)
#else
#    define XPTI_PUBLIC_API(t)    PR_IMPLEMENT(t)
#    define XPTI_PUBLIC_DATA(t)   t
#    define XPTI_EXPORT
#endif
#endif
#define XPTI_FRIEND_API(t)    XPTI_PUBLIC_API(t)
#define XPTI_FRIEND_DATA(t)   XPTI_PUBLIC_DATA(t)

class nsIInterfaceInfoManager;
PR_BEGIN_EXTERN_C
// Even if this is a service, it is cool to provide a direct accessor
XPTI_PUBLIC_API(nsIInterfaceInfoManager*)
XPTI_GetInterfaceInfoManager();
PR_END_EXTERN_C

#endif /* xptiinfo_h___ */
