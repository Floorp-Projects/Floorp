/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

#ifndef _JULIAN_DEFINES_H
#define _JULIAN_DEFINES_H

#include "xp_mcom.h"
#include "nspr.h"

#ifdef PR_ASSERT
#undef PR_ASSERT
#endif

#define PR_ASSERT(a) assert(a)

#ifdef LIBJULIAN
    #define NOT_NULL(X)	X
#endif

/* 
** DLL Import/Export control
*/
#ifdef XP_WIN32
#ifdef LIBJULIAN
#define JULIAN_PUBLIC	__declspec(dllexport)
#else
#define JULIAN_PUBLIC	__declspec(dllimport)
#endif
#else
#define JULIAN_PUBLIC
#endif

#define JBOOL XP_Bool

#if defined(XP_WIN32)
#define JSTRING CString
#define JCOLOR  long

#elif defined(XP_UNIX)
#define stricmp strcasecmp

#elif defined(XP_MAC)
#define JSTRING char[]
#define JCOLOR  long
#endif

/* #define JRGB(r, g ,b)  ( (r & 0xfff) | ((g & 0xfff) << 12)) | ((b & 0xfff) << 24))) */
#define JRGB(r, g ,b)   ( (r & 0xff) | ((g & 0xff) << 8) | ((b & 0xff) << 16) )
#define JCRED(r)        ( (r      ) & 0xff )
#define JCGREEN(g)      ( (g >> 8 ) & 0xff )
#define JCBLUE(b)       ( (b >> 16) & 0xff )

typedef struct
{
   long x;
   long y;
} JPOINT;

typedef struct
{
    int iExternalLeading;
} JTEXTMETRIC;

#endif  /* _JULIAN_DEFINES_H */

