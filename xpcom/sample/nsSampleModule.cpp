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

#include "nsSample.h"

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the object nsSample
//
// What this does is defines a functions nsSampleConstructor which we
// will specific in the nsModuleComponentInfo table. This functions will
// be used the generic factory to create an instance of nsSample.
//
// NOTE: This creates an instance of nsSample by using the default
//		 constructor.
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSampleImpl)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, progid, and
// class name.
//
static nsModuleComponentInfo components[] =
{
  { NS_SAMPLE_CID, &nsSampleImplConstructor, NS_SAMPLE_PROGID,
    "Sample Component" },
};

////////////////////////////////////////////////////////////////////////
// Implement the Module object
//
NS_IMPL_MODULE(nsSampleModule, components)

////////////////////////////////////////////////////////////////////////
// Implment the NSGetModule() exported function for your module
//
NS_IMPL_NSGETMODULE(nsSampleModule)
