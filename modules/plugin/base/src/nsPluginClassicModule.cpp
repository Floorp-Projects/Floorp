/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"
#include "nsIClassicPluginFactory.h"
#include "ns4xPlugin.h"

class nsClassicPluginFactory : public nsIClassicPluginFactory {
public:
    NS_DECL_ISUPPORTS
    
    nsClassicPluginFactory();
    
    static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_IMETHOD CreatePlugin(nsIServiceManagerObsolete* aServiceMgr, const char* aFileName,
                            PRLibrary* aLibrary, nsIPlugin** aResult);
};

NS_IMPL_ISUPPORTS1(nsClassicPluginFactory, nsIClassicPluginFactory)

nsClassicPluginFactory::nsClassicPluginFactory()
{
    NS_INIT_ISUPPORTS();
}

nsresult nsClassicPluginFactory::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsCOMPtr<nsIClassicPluginFactory> factory = new nsClassicPluginFactory;
    if (! factory)
        return NS_ERROR_OUT_OF_MEMORY;

    return factory->QueryInterface(aIID, aResult);
}

NS_METHOD nsClassicPluginFactory::CreatePlugin(nsIServiceManagerObsolete* aServiceMgr, const char* aFileName,
                                               PRLibrary* aLibrary, nsIPlugin** aResult)
{
    return ns4xPlugin::CreatePlugin(aServiceMgr, aFileName, aLibrary, aResult);
}

static nsModuleComponentInfo gComponentInfo[] = {
  { "Classic Plugin Factory",
    NS_CLASSIC_PLUGIN_FACTORY_CID,
    NS_CLASSIC_PLUGIN_FACTORY_CONTRACTID,
    nsClassicPluginFactory::Create },
};

PR_STATIC_CALLBACK(void)
nsPluginModuleDtor(nsIModule *self)
{
  ns4xPlugin::ReleaseStatics();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsClassicPluginModule, gComponentInfo,
                              nsPluginModuleDtor);
