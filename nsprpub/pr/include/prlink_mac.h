/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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


#ifndef prlink_mac_h___
#define prlink_mac_h___

#ifdef XP_MAC

#include <Files.h>
#include "prtypes.h"

PR_BEGIN_EXTERN_C

/*
** PR_LoadNamedFragment
** 
** Load a code fragment by fragment name from the data fork of the specified file.
** The fragment name is an internal name which uniquely identifies a code
** fragment; this call opens the 'cfrg' resource in the file to find the
** offsets of the named fragment.
** 
** If the specified fragment exists, it is loaded and an entry created
** in the load map (keyed by fragment name).
** 
** If fileSpec points to an alias, the alias is resolved by this call.
*/
NSPR_API(PRLibrary*) PR_LoadNamedFragment(const FSSpec *fileSpec, const char* fragmentName);

/*
** PR_LoadIndexedFragment
** 
** Load a code fragment by fragment index from the data fork of the specified file
** (since Mac shared libraries can contain multiple code fragments).
** This call opens the 'cfrg' resource in the file to find the offsets
** of the named fragment.
** 
** If the specified fragment exists, it is loaded and an entry created
** in the load map (keyed by fragment name).
** 
** If fileSpec points to an alias, the alias is resolved by this call.
** 
*/
NSPR_API(PRLibrary*) PR_LoadIndexedFragment(const FSSpec *fileSpec, PRUint32 fragIndex);

PR_END_EXTERN_C

#endif


#endif /* prlink_mac_h___ */
