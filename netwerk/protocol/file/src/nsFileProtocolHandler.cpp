/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:ts=4 sw=4 sts=4 et cin:
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsFileProtocolHandler.h"
#include "nsFileChannel.h"
#include "nsInputStreamChannel.h"
#include "nsStandardURL.h"
#include "nsURLHelper.h"
#include "nsNetCID.h"

#include "nsIServiceManager.h"
#include "nsIURL.h"

#include "nsNetUtil.h"

// URL file handling, copied and modified from xpfe/components/bookmarks/src/nsBookmarksService.cpp
#ifdef XP_WIN
#include <shlobj.h>
#include <intshcut.h>
#include "nsIFileURL.h"
#ifdef CompareString
#undef CompareString
#endif
#endif

//-----------------------------------------------------------------------------

nsFileProtocolHandler::nsFileProtocolHandler()
{
}

nsresult
nsFileProtocolHandler::Init()
{
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsFileProtocolHandler,
                              nsIFileProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI)
{
// IUniformResourceLocator isn't supported by VC5 (bless its little heart)
#if !defined(XP_WIN) || _MSC_VER < 1200
    return NS_ERROR_NOT_AVAILABLE;
#else
    nsAutoString path;
    rv = aFile->GetPath(path);
    if (NS_FAILED(rv))
        return rv;

    if (path.Length() < 4)
        return NS_ERROR_NOT_AVAILABLE;
    if (!StringTail(path, 4).LowerCaseEqualsLiteral(".url"))
        return NS_ERROR_NOT_AVAILABLE;

    HRESULT result;

    rv = NS_ERROR_NOT_AVAILABLE;

    IUniformResourceLocator* urlLink = nsnull;
    result = ::CoCreateInstance(CLSID_InternetShortcut, NULL, CLSCTX_INPROC_SERVER,
                                IID_IUniformResourceLocator, (void**)&urlLink);
    if (SUCCEEDED(result) && urlLink) {
        IPersistFile* urlFile = nsnull;
        result = urlLink->QueryInterface(IID_IPersistFile, (void**)&urlFile);
        if (SUCCEEDED(result) && urlFile) {
            result = urlFile->Load(path.get(), STGM_READ);
            if (SUCCEEDED(result) ) {
                LPSTR lpTemp = nsnull;

                // The URL this method will give us back seems to be already
                // escaped. Hence, do not do escaping of our own.
                result = urlLink->GetURL(&lpTemp);
                if (SUCCEEDED(result) && lpTemp) {
                    rv = NS_NewURI(aURI, lpTemp);

                    // free the string that GetURL alloc'd
                    IMalloc* pMalloc;
                    result = SHGetMalloc(&pMalloc);
                    if (SUCCEEDED(result)) {
                        pMalloc->Free(lpTemp);
                        pMalloc->Release();
                    }
                }
            }
            urlFile->Release();
        }
        urlLink->Release();
    }
    return rv;

#endif
}

NS_IMETHODIMP
nsFileProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("file");
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for file: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NOAUTH;
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::NewURI(const nsACString &spec,
                              const char *charset,
                              nsIURI *baseURI,
                              nsIURI **result)
{
    nsCOMPtr<nsIStandardURL> url = new nsStandardURL(PR_TRUE);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = url->Init(nsIStandardURL::URLTYPE_NO_AUTHORITY, -1,
                            spec, charset, baseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::NewChannel(nsIURI *uri, nsIChannel **result)
{
    // This file may be a url file
    nsCOMPtr<nsIFileURL> url(do_QueryInterface(uri));
    if (url) {
        nsCOMPtr<nsIFile> file;
        nsresult rv = url->GetFile(getter_AddRefs(file));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIURI> uri;
            rv = ReadURLFile(file, getter_AddRefs(uri));
            if (NS_SUCCEEDED(rv)) {
                rv = NS_NewChannel(result, uri);
                if (NS_SUCCEEDED(rv))
                    return rv;
            }
        }
    }

    nsFileChannel *chan = new nsFileChannel();
    if (!chan)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(chan);

    nsresult rv = chan->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(chan);
        return rv;
    }

    *result = chan;
    return NS_OK;
}

NS_IMETHODIMP 
nsFileProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *result)
{
    // don't override anything.  
    *result = PR_FALSE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIFileProtocolHandler methods:

NS_IMETHODIMP
nsFileProtocolHandler::NewFileURI(nsIFile *file, nsIURI **result)
{
    nsresult rv;

    nsCOMPtr<nsIFileURL> url = new nsStandardURL(PR_TRUE);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    // XXX shouldn't we set nsIURI::originCharset ??

    rv = url->SetFile(file);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromFile(nsIFile *file, nsACString &result)
{
    return net_GetURLSpecFromFile(file, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetFileFromURLSpec(const nsACString &spec, nsIFile **result)
{
    return net_GetFileFromURLSpec(spec, result);
}
