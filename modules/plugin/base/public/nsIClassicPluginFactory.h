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

#ifndef nsIClassicPluginFactory_h___
#define nsIClassicPluginFactory_h___

#include "nsplugindefs.h"
#include "nsISupports.h"

// {07bfa284-1dd2-11b2-90f8-fef5608e8a56}
#define NS_ICLASSICPLUGINFACTORY_IID \
{ 0x07bfa284, 0x1dd2, 0x11b2, { 0x90, 0xf8, 0xfe, 0xf5, 0x60, 0x8e, 0x8a, 0x56 } }

// {a55d21ca-1dd1-11b2-9b0f-8fe1adaf129d}
#define NS_CLASSIC_PLUGIN_FACTORY_CID \
{ 0xa55d21ca, 0x1dd1, 0x11b2, { 0x9b, 0x0f, 0x8f, 0xe1, 0xad, 0xaf, 0x12, 0x9d } }

// Prefix for ContractID of all plugins
#define NS_CLASSIC_PLUGIN_FACTORY_CONTRACTID "@mozilla.org/plugin/classicpluginfactory;1"

struct PRLibrary;

class nsIClassicPluginFactory : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICLASSICPLUGINFACTORY_IID)

   /**
    * A factory method for constructing 4.x plugins. Constructs
    * and initializes an ns4xPlugin object, and returns it in
    * <b>result</b>.
    */
    NS_IMETHOD CreatePlugin(nsIServiceManager* aServiceMgr, const char* aFileName,
                            PRLibrary* aLibrary, nsIPlugin** aResult) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIClassicPluginFactory_h___ */
