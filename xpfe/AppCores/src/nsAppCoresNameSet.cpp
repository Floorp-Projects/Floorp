
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

////////////////////////////////////////////////////////////////////////////////
// nsAppCoresNameSet
////////////////////////////////////////////////////////////////////////////////
#include "nsAppCoresNameSet.h"
#include "nsIScriptContext.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIDOMAppCoresManager.h"
#include "nsIDOMToolkitCore.h"
#include "nsIDOMMailCore.h"
#include "nsIDOMPrefsCore.h"
#include "nsIDOMRDFCore.h"
#include "nsIDOMToolbarCore.h"
#include "nsIDOMBrowserAppCore.h"
#include "nsIDOMEditorAppCore.h"
#include "nsAppCoresCIDs.h" 


static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);
static NS_DEFINE_IID(kAppCoresCID,           NS_APPCORESMANAGER_CID);
static NS_DEFINE_IID(kToolkitCoreCID,        NS_TOOLKITCORE_CID);
static NS_DEFINE_IID(kMailCoreCID,           NS_MAILCORE_CID);
static NS_DEFINE_IID(kPrefsCoreCID,          NS_PREFSCORE_CID);
static NS_DEFINE_IID(kRDFCoreCID,            NS_RDFCORE_CID);
static NS_DEFINE_IID(kToolbarCoreCID,        NS_TOOLBARCORE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID,     NS_BROWSERAPPCORE_CID);
static NS_DEFINE_IID(kEditorAppCoreCID,      NS_EDITORAPPCORE_CID);

nsAppCoresNameSet::nsAppCoresNameSet()
{
  NS_INIT_REFCNT();
}

nsAppCoresNameSet::~nsAppCoresNameSet()
{
}

NS_IMPL_ISUPPORTS(nsAppCoresNameSet, kIScriptExternalNameSetIID);




NS_IMETHODIMP
nsAppCoresNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;

    result = NS_InitAppCoresManagerClass(aScriptContext, nsnull);
    if (NS_OK != result) return result;

    result = NS_InitMailCoreClass(aScriptContext, nsnull);
    result = NS_InitPrefsCoreClass(aScriptContext, nsnull);
    result = NS_InitToolbarCoreClass(aScriptContext, nsnull);
    result = NS_InitBrowserAppCoreClass(aScriptContext, nsnull);
    result = NS_InitEditorAppCoreClass(aScriptContext, nsnull);
    result = NS_InitToolkitCoreClass(aScriptContext, nsnull);
    result = NS_InitRDFCoreClass(aScriptContext, nsnull);

    return result;
}




NS_IMETHODIMP
nsAppCoresNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;
    nsIScriptNameSpaceManager* manager;

    result = aScriptContext->GetNameSpaceManager(&manager);
    if (NS_OK == result) 
    {
        result = manager->RegisterGlobalName("MailCore", 
                                             kMailCoreCID, 
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("PrefsCore", 
                                             kPrefsCoreCID, 
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("RDFCore", 
                                             kRDFCoreCID, 
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("ToolbarCore", 
                                             kToolbarCoreCID, 
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("ToolkitCore",
                                             kToolkitCoreCID,
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("BrowserAppCore", 
                                             kBrowserAppCoreCID, 
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("EditorAppCore", 
                                             kEditorAppCoreCID, 
                                             PR_TRUE);

        if (NS_OK != result) return result;

        result = manager->RegisterGlobalName("XPAppCoresManager", 
                                             kAppCoresCID, 
                                             PR_FALSE);
        
        NS_RELEASE(manager);
    }
    return result;
}

