/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/23/2000   IBM Corp.       Fixed bug with OS2_SystemDirectory.
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsResProtocolHandler.h"
#include "nsAutoLock.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsResChannel.h"
#include "prenv.h"
#include "prmem.h"
#include "prprf.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsResProtocolHandler::nsResProtocolHandler()
    : mLock(nsnull), mSubstitutions(32)
{
    NS_INIT_REFCNT();
}

nsresult
nsResProtocolHandler::SetSpecialDir(const char* rootName, const char* sysDir)
{
    nsresult rv;
    nsCOMPtr<nsIFile> file;
    rv = NS_GetSpecialDirectory(sysDir, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFileURL> fileURL;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIFileURL),
                                            getter_AddRefs(fileURL));
    if (NS_FAILED(rv)) return rv;

    rv = fileURL->SetFile(file);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString spec;
    rv = fileURL->GetSpec(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    return AppendSubstitution(rootName, spec);
}

nsresult
nsResProtocolHandler::Init()
{
    nsresult rv;

    mLock = PR_NewLock();
    if (mLock == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    // set up initial mappings
    rv = SetSpecialDir("ProgramDir", NS_OS_CURRENT_PROCESS_DIR);
    if (NS_FAILED(rv)) return rv;

    // make "res:///" == "resource:/"
    rv = SetSpecialDir("", NS_XPCOM_CURRENT_PROCESS_DIR);
    if (NS_FAILED(rv)) return rv;

    rv = SetSpecialDir("CurrentDir", NS_OS_CURRENT_WORKING_DIR);
    if (NS_FAILED(rv)) return rv;

    rv = SetSpecialDir("CurrentDrive", NS_OS_DRIVE_DIR);
    if (NS_FAILED(rv)) return rv;

    rv = SetSpecialDir("TempDir", NS_OS_TEMP_DIR);
    if (NS_FAILED(rv)) return rv;

    rv = SetSpecialDir("ComponentsDir", NS_XPCOM_COMPONENT_DIR);
    if (NS_FAILED(rv)) return rv;

    rv = SetSpecialDir("SystemDir",
#ifdef XP_MAC
                       NS_OS_SYSTEM_DIR
#elif XP_OS2
                       NS_OS_SYSTEM_DIR
#elif XP_PC
                       NS_OS_SYSTEM_DIR
#elif XP_BEOS
                       NS_OS_SYSTEM_DIR
#else
                       NS_UNIX_LIB_DIR  // XXX ???
#endif
                       );
    if (NS_FAILED(rv)) return rv;

    // Set up the "Resource" root to point to the old resource location 
    // such that:
    //     resource://<path>  ==  res://Resource/<path>
    rv = SetSpecialDir("Resource", NS_XPCOM_CURRENT_PROCESS_DIR);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

nsResProtocolHandler::~nsResProtocolHandler()
{
    if (mLock)
        PR_DestroyLock(mLock);
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsResProtocolHandler, nsIResProtocolHandler, nsIProtocolHandler, nsISupportsWeakReference)

NS_METHOD
nsResProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsResProtocolHandler* ph = new nsResProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsResProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("resource");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for res: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsresult rv;

    // Res: URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsIURI* url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);
    if (NS_FAILED(rv)) return rv;

    if (aBaseURI) {
        nsXPIDLCString aResolvedURI;
        rv = aBaseURI->Resolve(aSpec, getter_Copies(aResolvedURI));
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec(aResolvedURI);
    } else {
        rv = url->SetSpec((char*)aSpec);
    }

    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsResProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsresult rv;
    
    nsResChannel* channel;
    rv = nsResChannel::Create(nsnull, NS_GET_IID(nsIResChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(this, uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsResProtocolHandler::PrependSubstitution(const char *root, const char *urlStr)
{
    nsresult rv;
    nsAutoLock lock(mLock);

    nsCOMPtr<nsIIOService> ioServ = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIURI> url;
    rv = ioServ->NewURI(urlStr, nsnull, getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;

    nsCStringKey key(root);
    nsCOMPtr<nsISupportsArray> strings;
    nsCOMPtr<nsISupportsArray> newStrings;

    strings = getter_AddRefs((nsISupportsArray*)mSubstitutions.Get(&key));
    if (strings) {
        // we have to snapshot the array when inserting a new element because
        // someone could be iterating over the existing array
        rv = strings->Clone(getter_AddRefs(newStrings));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        rv = NS_NewISupportsArray(getter_AddRefs(newStrings));
        if (NS_FAILED(rv)) return rv;
    }

    rv = newStrings->InsertElementAt(url, 0) ? NS_OK : NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(rv)) {
        // replace existing array
        (void)mSubstitutions.Put(&key, newStrings);
    }
    return rv;
}

NS_IMETHODIMP
nsResProtocolHandler::AppendSubstitution(const char *root, const char *urlStr)
{
    nsresult rv;
    nsAutoLock lock(mLock);

    nsCOMPtr<nsIIOService> ioServ = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIURI> url;
    rv = ioServ->NewURI(urlStr, nsnull, getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;

    nsCStringKey key(root);
    nsCOMPtr<nsISupportsArray> strings;
    nsCOMPtr<nsISupportsArray> newStrings;

    strings = getter_AddRefs((nsISupportsArray*)mSubstitutions.Get(&key));
    if (strings) {
        // we have to snapshot the array when inserting a new element because
        // someone could be iterating over the existing array
        rv = strings->Clone(getter_AddRefs(newStrings));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        rv = NS_NewISupportsArray(getter_AddRefs(newStrings));
        if (NS_FAILED(rv)) return rv;
    }

    rv = newStrings->AppendElement(url) ? NS_OK : NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(rv)) {
        // replace existing array
        (void)mSubstitutions.Put(&key, newStrings);
    }
    return rv;
}

struct CopyMostArgs {
    nsISupportsArray* mNewArray;
    nsIURI* mRemoveURL;
};

static PRBool
CopyMost(nsISupports* aElement, void *aData)
{
    nsresult rv = NS_OK;
    CopyMostArgs* args = (CopyMostArgs*)aData;
    nsIURI* thisURL = (nsIURI*)aElement;
    PRBool eq;
    rv = thisURL->Equals(args->mRemoveURL, &eq);
    if (NS_FAILED(rv)) return PR_FALSE;
    if (!eq) {
        rv = args->mNewArray->AppendElement(thisURL) ? NS_OK : NS_ERROR_FAILURE;
    }
    return NS_SUCCEEDED(rv);
}

NS_IMETHODIMP
nsResProtocolHandler::RemoveSubstitution(const char *root, const char *urlStr)
{
    nsresult rv;
    nsAutoLock lock(mLock);

    nsCOMPtr<nsIIOService> ioServ = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIURI> url;
    rv = ioServ->NewURI(urlStr, nsnull, getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;

    nsCStringKey key(root);
    nsCOMPtr<nsISupportsArray> strings;
    nsCOMPtr<nsISupportsArray> newStrings;

    strings = getter_AddRefs((nsISupportsArray*)mSubstitutions.Get(&key));
    if (strings == nsnull) {
        // can't remove element if it doesn't exist
        return NS_ERROR_FAILURE;
    }

    rv = NS_NewISupportsArray(getter_AddRefs(newStrings));
    if (NS_FAILED(rv)) return rv;

    CopyMostArgs args;
    args.mNewArray = newStrings;
    args.mRemoveURL = url;
    PRBool ok = strings->EnumerateForwards(CopyMost, &args);
    rv = ok ? NS_OK : NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(rv)) {
        // replace existing array
        (void)mSubstitutions.Put(&key, newStrings);
    }
    return rv;
}

NS_IMETHODIMP
nsResProtocolHandler::GetSubstitutions(const char *root, nsISupportsArray* *result)
{
    nsresult rv;
    nsAutoLock lock(mLock);

    nsCStringKey key(root);
    nsISupportsArray* strings = (nsISupportsArray*)mSubstitutions.Get(&key);
    if (strings == nsnull) {
        rv = NS_NewISupportsArray(&strings);
        if (NS_FAILED(rv)) return rv;
        (void)mSubstitutions.Put(&key, strings);
    }
    *result = strings;
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::HasSubstitutions(const char *root, PRBool *result)
{
    nsCStringKey key(root);
    *result = mSubstitutions.Exists(&key);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
