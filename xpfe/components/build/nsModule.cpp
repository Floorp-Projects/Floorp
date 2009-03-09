/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"
#include "nsICategoryManager.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
#include "nsDirectoryViewer.h"
#ifdef MOZ_RDF
#include "rdf.h"
#include "nsRDFCID.h"
#endif

#ifdef SUITE_USING_XPFE_DM
#include "nsDownloadManager.h"
#include "nsDownloadProxy.h"
#endif

#if !defined(MOZ_MACBROWSER)
#include "nsBrowserStatusFilter.h"
#include "nsBrowserInstance.h"
#endif
#include "nsCURILoader.h"
#include "nsXPFEComponentsCID.h"

#ifdef MOZ_RDF
// Factory constructors
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHTTPIndex, Init)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDirectoryViewerFactory)

#if !defined(MOZ_MACBROWSER)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserStatusFilter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserInstance)
#endif

#ifdef SUITE_USING_XPFE_DM
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDownloadManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloadProxy)
#endif

static NS_METHOD
RegisterProc(nsIComponentManager *aCompMgr,
             nsIFile *aPath,
             const char *registryLocation,
             const char *componentType,
             const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // add the MIME types layotu can handle to the handlers category.
    // this allows users of layout's viewers (the docshell for example)
    // to query the types of viewers layout can create.
    return catman->AddCategoryEntry("Gecko-Content-Viewers", "application/http-index-format",
                                    "@mozilla.org/xpfe/http-index-format-factory-constructor",
                                    PR_TRUE, PR_TRUE, nsnull);
}

static NS_METHOD
UnregisterProc(nsIComponentManager *aCompMgr,
               nsIFile *aPath,
               const char *registryLocation,
               const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    return catman->DeleteCategoryEntry("Gecko-Content-Viewers",
                                       "application/http-index-format", PR_TRUE);
}

static const nsModuleComponentInfo components[] = {
   { "Directory Viewer", NS_DIRECTORYVIEWERFACTORY_CID,
      "@mozilla.org/xpfe/http-index-format-factory-constructor",
      nsDirectoryViewerFactoryConstructor, RegisterProc, UnregisterProc  },
#ifdef MOZ_RDF
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_SERVICE_CONTRACTID,
      nsHTTPIndexConstructor },
    { "Directory Viewer", NS_HTTPINDEX_SERVICE_CID, NS_HTTPINDEX_DATASOURCE_CONTRACTID,
      nsHTTPIndexConstructor },
#endif

#ifdef SUITE_USING_XPFE_DM
    { "Download Manager", NS_DOWNLOADMANAGER_CID, NS_DOWNLOADMANAGER_CONTRACTID,
      nsDownloadManagerConstructor },
    { "Download", NS_DOWNLOAD_CID, NS_TRANSFER_CONTRACTID,
      nsDownloadProxyConstructor },
#endif

#if !defined(MOZ_MACBROWSER)
    { NS_BROWSERSTATUSFILTER_CLASSNAME,
      NS_BROWSERSTATUSFILTER_CID,
      NS_BROWSERSTATUSFILTER_CONTRACTID,
      nsBrowserStatusFilterConstructor
    },
    { "nsBrowserInstance",
      NS_BROWSERINSTANCE_CID,
      NS_BROWSERINSTANCE_CONTRACTID,
      nsBrowserInstanceConstructor
    },
#endif

};

NS_IMPL_NSGETMODULE(application, components)
