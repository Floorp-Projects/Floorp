/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIModule.h"
#include "nsNativeComponentLoader.h"
 
struct nsStaticModuleInfo {
  const char      *name;
  nsGetModuleProc getModule;
};

// Must be implemented by some part of the app, if we're building the
// static component loader.
extern "C" {
typedef nsresult (PR_CALLBACK *NSGetStaticModuleInfoFunc)(nsStaticModuleInfo **info, PRUint32 *count);
extern NS_COM NSGetStaticModuleInfoFunc NSGetStaticModuleInfo;
}

