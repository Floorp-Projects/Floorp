/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
