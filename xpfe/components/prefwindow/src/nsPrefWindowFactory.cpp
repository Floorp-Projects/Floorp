/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
