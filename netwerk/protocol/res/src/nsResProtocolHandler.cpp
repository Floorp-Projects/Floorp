/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include "nsResProtocolHandler.h"
#include "nsAutoLock.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "prenv.h"
#include "prmem.h"
#include "prprf.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static nsResProtocolHandler *gResHandler = nsnull;

#if defined(PR_LOGGING)
//
// Log module for Resource Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsResProtocol:5
//    set NSPR_LOG_FILE=log.txt
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file log.txt
//
static PRLogModuleInfo *gResLog;
#endif
#define LOG(args) PR_LOG(gResLog, PR_LOG_DEBUG, args)

//----------------------------------------------------------------------------
// nsResURL : overrides nsStdURL::GetFile to provide nsIFile resolution
//----------------------------------------------------------------------------

#include "nsStdURL.h"

class nsResURL : public nsStdURL
{
public:
    NS_IMETHOD GetFile(nsIFile **);
};

NS_IMETHODIMP
nsResURL::GetFile(nsIFile **result)
{
    nsresult rv;

    NS_ENSURE_TRUE(gResHandler, NS_ERROR_NOT_AVAILABLE);

    nsXPIDLCString spec;
    rv = gResHandler->ResolveURI(this, getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILocalFile> localFile =
            do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = localFile->SetURL(spec);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(localFile, result);
}

//----------------------------------------------------------------------------
// nsResProtocolHandler <public>
//----------------------------------------------------------------------------

nsResProtocolHandler::nsResProtocolHandler()
    : mSubstitutions(32)
{
    NS_INIT_REFCNT();

#if defined(PR_LOGGING)
    gResLog = PR_NewLogModule("nsResProtocol");
#endif

    NS_ASSERTION(!gResHandler, "res handler already created!");
    gResHandler = this;
}

nsResProtocolHandler::~nsResProtocolHandler()
{
    gResHandler = nsnull;
}

nsresult
nsResProtocolHandler::Init()
{
    nsresult rv;

    mIOService = do_GetIOService(&rv);
    if (NS_FAILED(rv)) return rv;

    // set up initial mappings
    rv = SetSpecialDir("programdir", NS_OS_CURRENT_PROCESS_DIR);
    if (NS_FAILED(rv)) return rv;

    // make "res:///" == "resource:/"
    rv = SetSpecialDir("", NS_XPCOM_CURRENT_PROCESS_DIR);
    if (NS_FAILED(rv)) return rv;

    rv = SetSpecialDir("tempdir", NS_OS_TEMP_DIR);
    if (NS_FAILED(rv)) return rv;

    rv = SetSpecialDir("componentsdir", NS_XPCOM_COMPONENT_DIR);
    if (NS_FAILED(rv)) return rv;

    // Set up the "Resource" root to point to the old resource location 
    // such that:
    //     resource://<path>  ==  res://Resource/<path>
    rv = SetSpecialDir("resource", NS_XPCOM_CURRENT_PROCESS_DIR);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

//----------------------------------------------------------------------------
// nsResProtocolHandler <private>
//----------------------------------------------------------------------------

nsresult
nsResProtocolHandler::SetSpecialDir(const char *root, const char *dir)
{
    LOG(("nsResProtocolHandler::SetSpecialDir [root=\"%s\" dir=%s]\n", root, dir));

    nsresult rv;
    nsCOMPtr<nsIFile> file;
    rv = NS_GetSpecialDirectory(dir, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString spec;
    rv = file->GetURL(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    LOG(("root=\"%s\" -> baseURI=%s\n", root, spec.get()));

    nsCOMPtr<nsIURI> uri;
    mIOService->NewURI(spec, nsnull, getter_AddRefs(uri));

    return SetSubstitution(root, uri);
}

//----------------------------------------------------------------------------
// nsResProtocolHandler::nsISupports
//----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS3(nsResProtocolHandler,
                              nsIResProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference)

//----------------------------------------------------------------------------
// nsResProtocolHandler::nsIProtocolHandler
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsResProtocolHandler::GetScheme(char **result)
{
    NS_ENSURE_ARG_POINTER(result);
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
nsResProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD;
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsresult rv;

    nsResURL *resURL;
    NS_NEWXPCOM(resURL, nsResURL);
    if (!resURL)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(resURL);

    rv = resURL->Init(nsIStandardURL::URLTYPE_STANDARD, -1, aSpec, aBaseURI);
    if (NS_SUCCEEDED(rv))
        rv = CallQueryInterface(resURL, result);
    NS_RELEASE(resURL);
    return rv;
}

NS_IMETHODIMP
nsResProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsresult rv;
    nsXPIDLCString spec;

    rv = ResolveURI(uri, getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    rv = mIOService->NewChannel(spec, nsnull, result);
    if (NS_FAILED(rv)) return rv;

    return (*result)->SetOriginalURI(uri);
}

NS_IMETHODIMP 
nsResProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsResProtocolHandler::nsIResProtocolHandler
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsResProtocolHandler::SetSubstitution(const char *root, nsIURI *baseURI)
{
    NS_ENSURE_ARG_POINTER(root);

    nsCStringKey key(root);
    if (baseURI)
        mSubstitutions.Put(&key, baseURI);
    else
        mSubstitutions.Remove(&key);
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::GetSubstitution(const char *root, nsIURI **result)
{
    NS_ENSURE_ARG_POINTER(root);
    NS_ENSURE_ARG_POINTER(result);

    nsCStringKey key(root);
    *result = NS_STATIC_CAST(nsIURI *, mSubstitutions.Get(&key));
    return *result ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsResProtocolHandler::HasSubstitution(const char *root, PRBool *result)
{
    NS_ENSURE_ARG_POINTER(root);
    NS_ENSURE_ARG_POINTER(result);

    nsCStringKey key(root);
    *result = mSubstitutions.Exists(&key);
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::ResolveURI(nsIURI *uri, char **result)
{
    nsresult rv;
    nsXPIDLCString host, path;

    rv = uri->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    rv = uri->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> baseURI;
    rv = GetSubstitution(host.get() ?  host.get() : "", getter_AddRefs(baseURI));
    if (NS_FAILED(rv)) return rv;

    const char *p = path.get(); // be nice to the AIX and OS/2 compilers
    rv = baseURI->Resolve(p[0] == '/' ? p+1 : p, result);

#if defined(PR_LOGGING)
    if (PR_LOG_TEST(gResLog, PR_LOG_DEBUG)) {
        nsXPIDLCString spec;
        uri->GetSpec(getter_Copies(spec));
        LOG(("%s\n -> %s\n", spec.get(), *result));
    }
#endif
    return rv;
}
