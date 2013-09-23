/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:ts=4 sw=4 sts=4 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFileProtocolHandler.h"
#include "nsFileChannel.h"
#include "nsStandardURL.h"
#include "nsURLHelper.h"

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

// URL file handling for OS/2
#ifdef XP_OS2
#include "prio.h"
#include "nsIFileURL.h"
#include "nsILocalFileOS2.h"
#endif

// URL file handling for freedesktop.org
#ifdef XP_UNIX
#include "nsINIParser.h"
#define DESKTOP_ENTRY_SECTION "Desktop Entry"
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

NS_IMPL_ISUPPORTS3(nsFileProtocolHandler,
                   nsIFileProtocolHandler,
                   nsIProtocolHandler,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsIProtocolHandler methods:

#if defined(XP_WIN)
NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI)
{
    nsAutoString path;
    nsresult rv = aFile->GetPath(path);
    if (NS_FAILED(rv))
        return rv;

    if (path.Length() < 4)
        return NS_ERROR_NOT_AVAILABLE;
    if (!StringTail(path, 4).LowerCaseEqualsLiteral(".url"))
        return NS_ERROR_NOT_AVAILABLE;

    HRESULT result;

    rv = NS_ERROR_NOT_AVAILABLE;

    IUniformResourceLocatorW* urlLink = nullptr;
    result = ::CoCreateInstance(CLSID_InternetShortcut, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IUniformResourceLocatorW, (void**)&urlLink);
    if (SUCCEEDED(result) && urlLink) {
        IPersistFile* urlFile = nullptr;
        result = urlLink->QueryInterface(IID_IPersistFile, (void**)&urlFile);
        if (SUCCEEDED(result) && urlFile) {
            result = urlFile->Load(path.get(), STGM_READ);
            if (SUCCEEDED(result) ) {
                LPWSTR lpTemp = nullptr;

                // The URL this method will give us back seems to be already
                // escaped. Hence, do not do escaping of our own.
                result = urlLink->GetURL(&lpTemp);
                if (SUCCEEDED(result) && lpTemp) {
                    rv = NS_NewURI(aURI, nsDependentString(lpTemp));
                    // free the string that GetURL alloc'd
                    CoTaskMemFree(lpTemp);
                }
            }
            urlFile->Release();
        }
        urlLink->Release();
    }
    return rv;
}

#elif defined(XP_OS2)
NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI)
{
    nsresult rv;

    nsCOMPtr<nsILocalFileOS2> os2File (do_QueryInterface(aFile, &rv));
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;

    // see if this file is a WPS UrlObject
    bool isUrl;
    rv = os2File->IsFileType(NS_LITERAL_CSTRING("UniformResourceLocator"),
                             &isUrl);
    if (NS_FAILED(rv) || !isUrl)
        return NS_ERROR_NOT_AVAILABLE;

    // if so, open it & get its size
    PRFileDesc *file;
    rv = os2File->OpenNSPRFileDesc(PR_RDONLY, 0, &file);
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;

    int64_t fileSize;
    os2File->GetFileSize(&fileSize);
    rv = NS_ERROR_NOT_AVAILABLE;

    // get a buffer, read the entire file, then create
    // an nsURI;  we assume the string is already escaped
    char * buffer = (char*)NS_Alloc(fileSize+1);
    if (buffer) {
        int32_t cnt = PR_Read(file, buffer, fileSize);
        if (cnt > 0) {
            buffer[cnt] = '\0';
            if (NS_SUCCEEDED(NS_NewURI(aURI, nsDependentCString(buffer))))
                rv = NS_OK;
        }
        NS_Free(buffer);
    }
    PR_Close(file);

    return rv;
}

#elif defined(XP_UNIX)
NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI)
{
    // We only support desktop files that end in ".desktop" like the spec says:
    // http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s02.html
    nsAutoCString leafName;
    nsresult rv = aFile->GetNativeLeafName(leafName);
    if (NS_FAILED(rv) ||
	!StringEndsWith(leafName, NS_LITERAL_CSTRING(".desktop")))
        return NS_ERROR_NOT_AVAILABLE;

    nsINIParser parser;
    rv = parser.Init(aFile);
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString type;
    parser.GetString(DESKTOP_ENTRY_SECTION, "Type", type);
    if (!type.EqualsLiteral("Link"))
        return NS_ERROR_NOT_AVAILABLE;

    nsAutoCString url;
    rv = parser.GetString(DESKTOP_ENTRY_SECTION, "URL", url);
    if (NS_FAILED(rv) || url.IsEmpty())
        return NS_ERROR_NOT_AVAILABLE;

    return NS_NewURI(aURI, url);
}

#else // other platforms
NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI)
{
    return NS_ERROR_NOT_AVAILABLE;
}
#endif // ReadURLFile()

NS_IMETHODIMP
nsFileProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("file");
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetDefaultPort(int32_t *result)
{
    *result = -1;        // no port for file: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetProtocolFlags(uint32_t *result)
{
    *result = URI_NOAUTH | URI_IS_LOCAL_FILE | URI_IS_LOCAL_RESOURCE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::NewURI(const nsACString &spec,
                              const char *charset,
                              nsIURI *baseURI,
                              nsIURI **result)
{
    nsCOMPtr<nsIStandardURL> url = new nsStandardURL(true);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    const nsACString *specPtr = &spec;

#if defined(XP_WIN) || defined(XP_OS2)
    nsAutoCString buf;
    if (net_NormalizeFileURL(spec, buf))
        specPtr = &buf;
#endif

    nsresult rv = url->Init(nsIStandardURL::URLTYPE_NO_AUTHORITY, -1,
                            *specPtr, charset, baseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::NewChannel(nsIURI *uri, nsIChannel **result)
{
    nsFileChannel *chan = new nsFileChannel(uri);
    if (!chan)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(chan);

    nsresult rv = chan->Init();
    if (NS_FAILED(rv)) {
        NS_RELEASE(chan);
        return rv;
    }

    *result = chan;
    return NS_OK;
}

NS_IMETHODIMP 
nsFileProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *result)
{
    // don't override anything.  
    *result = false;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIFileProtocolHandler methods:

NS_IMETHODIMP
nsFileProtocolHandler::NewFileURI(nsIFile *file, nsIURI **result)
{
    NS_ENSURE_ARG_POINTER(file);
    nsresult rv;

    nsCOMPtr<nsIFileURL> url = new nsStandardURL(true);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    // NOTE: the origin charset is assigned the value of the platform
    // charset by the SetFile method.
    rv = url->SetFile(file);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromFile(nsIFile *file, nsACString &result)
{
    NS_ENSURE_ARG_POINTER(file);
    return net_GetURLSpecFromFile(file, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromActualFile(nsIFile *file, 
                                                nsACString &result)
{
    NS_ENSURE_ARG_POINTER(file);
    return net_GetURLSpecFromActualFile(file, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromDir(nsIFile *file, nsACString &result)
{
    NS_ENSURE_ARG_POINTER(file);
    return net_GetURLSpecFromDir(file, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetFileFromURLSpec(const nsACString &spec, nsIFile **result)
{
    return net_GetFileFromURLSpec(spec, result);
}
