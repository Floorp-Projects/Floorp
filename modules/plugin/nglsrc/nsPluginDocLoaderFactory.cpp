/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIPluginHost.h"
#include "nsIPluginManager.h"
#include "nsIServiceManager.h"
#include "nsPluginDocLoaderFactory.h"
#include "nsPluginViewer.h"

NS_METHOD
nsPluginDocLoaderFactory::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aOuter == nsnull, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsPluginDocLoaderFactory* factory = new nsPluginDocLoaderFactory();
  if (! factory)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;
  NS_ADDREF(factory);
  rv = factory->QueryInterface(aIID, aResult);
  NS_RELEASE(factory);
  return rv;
}

NS_IMPL_ISUPPORTS1(nsPluginDocLoaderFactory, nsIDocumentLoaderFactory);

NS_IMETHODIMP
nsPluginDocLoaderFactory::CreateInstance(const char *aCommand,
                                         nsIChannel* aChannel,
                                         nsILoadGroup* aLoadGroup,
                                         const char* aContentType, 
                                         nsISupports* aContainer,
                                         nsISupports* aExtraInfo,
                                         nsIStreamListener** aDocListener,
                                         nsIContentViewer** aDocViewer)
{
  static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
  nsCOMPtr<nsIPluginHost> pluginHost = do_GetService(kPluginManagerCID);
  if(! pluginHost)
    return NS_ERROR_FAILURE;

  if (pluginHost->IsPluginEnabledForType(aContentType) != NS_OK)
    return NS_ERROR_FAILURE;

  return NS_NewPluginContentViewer(aCommand, aDocListener, aDocViewer);
}

NS_IMETHODIMP
nsPluginDocLoaderFactory::CreateInstanceForDocument(nsISupports* aContainer,
                                                    nsIDocument* aDocument,
                                                    const char *aCommand,
                                                    nsIContentViewer** aDocViewerResult)
{
  NS_NOTREACHED("how'd I get here?");
  return NS_ERROR_FAILURE;
}
