/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsAppCores_h___
#define nsAppCores_h___

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nscore.h"
#include "prio.h"

////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////
extern "C" NS_EXPORT nsresult
NSGetFactory(const nsCID &aClass, nsISupports* serviceMgr, nsIFactory **aFactory);

extern "C" NS_EXPORT PRBool
NSCanUnload(void);

extern "C" NS_EXPORT nsresult
NSRegisterSelf(const char *path);

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(const char *path);

extern "C" void
IncInstanceCount();

extern "C" void
IncLockCount();

extern "C" void
DecInstanceCount();

extern "C" void
DecLockCount();












#endif 
