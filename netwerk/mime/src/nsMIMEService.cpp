/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): Judson Valeski
 */

#include "nsMIMEService.h"
#include "nsVoidArray.h"
#include "nsString2.h"
#include "nsMIMEInfoImpl.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

// Hash table helper functions
PRBool DeleteEntry(nsHashKey *aKey, void *aData, void* closure) {
    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)aData;
    NS_ASSERTION(entry, "mapping problem");
	NS_RELEASE(entry);
    return PR_TRUE;   
};

// nsISupports methods
NS_IMPL_ISUPPORTS1(nsMIMEService, nsIMIMEService);

NS_METHOD
nsMIMEService::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {
    nsMIMEService* service = new nsMIMEService();
    if (!service) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(service);
    nsresult rv = service->Init();
    if (NS_FAILED(rv)) return rv;
    rv = service->QueryInterface(aIID, aResult);
    NS_RELEASE(service);
    return rv;
}

// nsMIMEService methods
nsMIMEService::nsMIMEService() {
    NS_INIT_REFCNT();
}

nsMIMEService::~nsMIMEService() {
    mInfoObjects->Reset(DeleteEntry, nsnull);
    delete mInfoObjects;
}

nsresult
nsMIMEService::Init() {
    mInfoObjects = new nsHashtable();
    if (!mInfoObjects) return NS_ERROR_OUT_OF_MEMORY;

    return InitFromHack();
}

 /* This bad boy needs to retrieve a url, and parse the data coming back, and
  * add entries into the mInfoArray.
  */
nsresult
nsMIMEService::InitFromURI(nsIURI *aUri) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsMIMEService::InitFromFile(const char *aFileName) {
#if 0
    nsFileSpec dirSpec;
    nsIFileSpec* spec = NS_LocateFileOrDirectory(nsSpecialFileSpec::App_UserProfileDirectory50);
    if (!spec) return NS_ERROR_FAILURE;
    spec->GetFileSpec(&dirSpec);

    nsInputFileStream inStream(dirSpec + aFileName);
    if (!inStream.is_open()) {
        return NS_OK;
    }

    // digest the file.
#endif
    return NS_ERROR_NOT_IMPLEMENTED;

}

// Call this first (perhaps only once) to create the initial nsMIMEInfo object
nsresult
nsMIMEService::AddMapping(const char* mimeType, 
                          const char* extension,
                          const char* description,
                          nsIURI* dataURI)
{
    // setup the new MIMEInfo object.
    nsMIMEInfoImpl* anInfo = new nsMIMEInfoImpl(mimeType);
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;

    anInfo->mExtensions.AppendString(extension);
    anInfo->mDescription = description;
    anInfo->mURI = dataURI;

    // The entry is mapped many-to-one.

    nsStringKey key(mimeType);
    nsMIMEInfoImpl* oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, anInfo);
    NS_IF_RELEASE(oldInfo);
    NS_ADDREF(anInfo);

    key = extension;
    oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, anInfo);
    NS_IF_RELEASE(oldInfo);
    NS_ADDREF(anInfo);

    return NS_OK;
}

// Call this for subsequent extensions to be mapped to the MIME type.
// You must call AddMapping() for a given MIME type *before* calling this.
nsresult
nsMIMEService::AppendExtension(const char* mimeType, const char* extension) {
    nsStringKey key(mimeType);

    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)mInfoObjects->Get(&key);
    NS_ASSERTION(info, "only call AppendExtension *after* a call to AddMapping");

    info->mExtensions.AppendString(extension);

    // Add another mapping.
    key = extension;
    nsMIMEInfoImpl* oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, info);
    NS_IF_RELEASE(oldInfo);
    NS_ADDREF(info);

    return NS_OK;
}

nsresult
nsMIMEService::InitFromHack() {
    nsresult rv;

    rv = AddMapping("text/plain", "txt", "Text File");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension("text/plain", "text");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("application/octet-stream", "exe", "Binary Executable");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension("application/octet-stream", "bin");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("text/html", "htm", "Hyper Text Markup Language");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension("text/html", "html");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension("text/html", "shtml");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("text/rdf", "rdf", "Resource Description Framework");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("text/xul", "xul", "XML-Based User Interface Language");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("text/xml", "xml", "Extensible Markup Language");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("text/css", "css", "Style Sheet");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("application/x-javascript", "js", "Javascript Source File");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("message/rfc822", "eml", "RFC-822 data");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("image/gif", "gif", "GIF Image");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("image/jpeg", "jpeg", "JPEG Image");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension("image/jpeg", "jpg");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("image/png", "png", "PNG Image");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("image/x-jg", "art", "ART Image");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping("image/tiff", "tiff", "TIFF Image");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension("image/tiff", "tif");
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}


// nsIMIMEService methods
NS_IMETHODIMP
nsMIMEService::GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval) {
    // for now we're assuming file extensions are case insensitive.
    nsCAutoString fileExt(aFileExt);
    fileExt.ToLowerCase();

    nsStringKey key(fileExt.GetBuffer());

    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoObjects->Get(&key);
    if (!entry) return NS_ERROR_FAILURE;
    NS_ADDREF(entry);
    *_retval = NS_STATIC_CAST(nsIMIMEInfo*, entry);
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEService::GetTypeFromExtension(const char *aFileExt, char **aContentType) {
    nsresult rv = NS_OK;;
    nsCOMPtr<nsIMIMEInfo> info;
    rv = GetFromExtension(aFileExt, getter_AddRefs(info));
    if (NS_FAILED(rv)) return rv;

    rv = info->GetMIMEType(aContentType);
    return rv;
}

NS_IMETHODIMP
nsMIMEService::GetTypeFromURI(nsIURI *aURI, char **aContentType) {
    nsresult rv = NS_ERROR_FAILURE;
    // first try to get a url out of the uri so we can skip post
    // filename stuff (i.e. query string)
    nsCOMPtr<nsIURL> url = do_QueryInterface(aURI, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsXPIDLCString ext;
        rv = url->GetFileExtension(getter_Copies(ext));
        if (NS_FAILED(rv)) return rv;
        rv = GetTypeFromExtension(ext, aContentType);
        return rv;
    }

    nsXPIDLCString cStrSpec;
    // no url, let's give the raw spec a shot
    rv = aURI->GetSpec(getter_Copies(cStrSpec));
    if (NS_FAILED(rv)) return rv;

    nsAutoString specStr(cStrSpec);

    // find the file extension (if any)
    nsAutoString extStr;
    PRInt32 extLoc = specStr.RFindChar('.');
    if (-1 != extLoc) {
        specStr.Right(extStr, specStr.Length() - extLoc - 1);
        char *ext = extStr.ToNewCString();
        if (!ext) return NS_ERROR_OUT_OF_MEMORY;
        rv = GetTypeFromExtension(ext, aContentType);
        nsAllocator::Free(ext);
    }
    else
        return NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP
nsMIMEService::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval) {
    nsCAutoString MIMEType(aMIMEType);
    MIMEType.ToLowerCase();

    nsStringKey key(MIMEType.GetBuffer());

    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoObjects->Get(&key);
    if (!entry) return NS_ERROR_FAILURE;
    NS_ADDREF(entry);
    *_retval = NS_STATIC_CAST(nsIMIMEInfo*, entry);
    return NS_OK;
}
