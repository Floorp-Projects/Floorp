/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsConsoleService.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsConsoleService)

static nsModuleComponentInfo components[] =
{
  { "Console Service", NS_CONSOLESERVICE_CID, NS_CONSOLESERVICE_PROGID,
    nsConsoleServiceConstructor,
    nsnull, // RegisterConsoleService
    nsnull, // UnregisterConsoleService
  }
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
// NOTE: If you want to use the module shutdown to release any
//		module specific resources, use the macro
//		NS_IMPL_NSGETMODULE_WITH_DTOR() instead of the vanilla
//		NS_IMPL_NSGETMODULE()
//

// e.g. xpconnect uses this to release some singletons;
// xdr search on ..._WITH_DTOR

NS_IMPL_NSGETMODULE("nsConsoleServiceModule", components)


