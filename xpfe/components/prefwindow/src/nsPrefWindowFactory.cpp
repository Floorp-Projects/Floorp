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

#include "nsIServiceManager.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "pratom.h"
#include "nsCOMPtr.h"

// include files for components this factory creates...
#include "nsPrefWindow.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrefWindow)

static nsModuleComponentInfo components[] = {
  { NS_PREFWINDOW_CID, &nsPrefWindowConstructor, NS_PREFWINDOW_PROGID, "Preferences window helper object", },
};

NS_IMPL_MODULE(nsPrefWindowModule, components)
NS_IMPL_NSGETMODULE(nsPrefWindowModule)
