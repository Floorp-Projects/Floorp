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
#include "nsString.h"
#include "nsMIMEInfoImpl.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"

// Hash table helper functions
PRBool DeleteEntry(nsHashKey *aKey, void *aData, void* closure) {
    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)aData;
    NS_ASSERTION(entry, "mapping problem");
	NS_RELEASE(entry);
    return PR_TRUE;   
};

// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMIMEService, nsIMIMEService);

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
    nsresult rv = NS_OK;
    mInfoObjects = new nsHashtable();
    if (!mInfoObjects) return NS_ERROR_OUT_OF_MEMORY;

    rv = NS_NewISupportsArray(getter_AddRefs(mInfoArray));
    if (NS_FAILED(rv)) return rv;

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

NS_IMETHODIMP
nsMIMEService::AddMapping(const char* mimeType, 
                          const char* extension,
                          const char* description,
                          nsIURI* dataURI)
{
    nsresult rv = NS_OK;
    // setup the new MIMEInfo object.
    nsMIMEInfoImpl* anInfo = new nsMIMEInfoImpl(mimeType);
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;

    anInfo->mExtensions.AppendCString(extension);
    anInfo->mDescription.AssignWithConversion(description);
    anInfo->mURI = dataURI;

    // The entry is mapped many-to-one and the MIME type is the root mapping.
    
    // First remove any existing mapping.
    rv = RemoveMapping(mimeType);
    if (NS_FAILED(rv)) return rv;

    // Next add the new root MIME mapping.
    nsStringKey key(mimeType);
    nsMIMEInfoImpl* oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, anInfo);
    NS_ASSERTION(!oldInfo, "we just removed the entry, we shouldn't have one");
    NS_ADDREF(anInfo);

    rv = mInfoArray->AppendElement(anInfo); // update the root array.
    if (NS_FAILED(rv)) return rv;

    // Finally add an extension mapping.
    key = extension;
    oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, anInfo);
    NS_ASSERTION(!oldInfo, "file extension mappings should have been cleaned up in the RemoveMapping call");
    NS_ADDREF(anInfo);

    return NS_OK;
}

// used to cleanup any file extension mappings when 
// a root MIME entry is removed.
PRBool removeExts(nsCString& aElement, void *aData) {
    nsHashtable* infoObjects = (nsHashtable*)aData;
    NS_ASSERTION(infoObjects, "hash table botched up");

    nsStringKey key(aElement);
    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)infoObjects->Remove(&key);
    NS_RELEASE(info);
    return PR_TRUE;
}

NS_IMETHODIMP
nsMIMEService::RemoveMapping(const char* aMIMEType) {
    nsresult rv = NS_OK;
    nsStringKey key(aMIMEType);

    // First remove the root MIME mapping.
    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)mInfoObjects->Remove(&key);
    if (!info) return NS_OK;

    rv = mInfoArray->RemoveElement(info); // update the root array.
    if (NS_FAILED(rv)) return rv;

    // Next remove any file association mappings.
    rv = info->mExtensions.EnumerateForwards(removeExts, mInfoObjects);
    NS_RELEASE(info);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEService::AppendExtension(const char* mimeType, const char* extension) {
    nsStringKey key(mimeType);

    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)mInfoObjects->Get(&key);
    if (!info) return NS_ERROR_FAILURE;

    info->mExtensions.AppendCString(extension);

    // Add another file extension mapping.
    key = extension;
    nsMIMEInfoImpl* oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, info);
    NS_IF_RELEASE(oldInfo); // overwrite any existing mapping.
    NS_ADDREF(info);

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEService::RemoveExtension(const char* aExtension) {
    nsresult rv = NS_OK;
    nsStringKey key(aExtension);

    // First remove the extension mapping.
    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)mInfoObjects->Remove(&key);
    if (!info) return NS_ERROR_FAILURE;
    
    // Next remove the root MIME mapping from the array and hash
    // IFF this was the only file extension mapping left.

      // STRING USE WARNING: this particular use really needs to be examined -- scc
    nsCAutoString keyString;
    keyString.AssignWithConversion(key.GetString().GetUnicode());
    PRBool removed = info->mExtensions.RemoveCString(keyString);
    NS_ASSERTION(removed, "mapping problem");

    if (info->GetExtCount() == 0) {
        // it's empty, remove the root mapping from hash and array.
        nsXPIDLCString mimeType;
        rv = info->GetMIMEType(getter_Copies(mimeType));
        if (NS_FAILED(rv)) return rv;

        key = (const char*)mimeType;
        nsMIMEInfoImpl* rootEntry = (nsMIMEInfoImpl*)mInfoObjects->Remove(&key);
        NS_ASSERTION(rootEntry, "mapping miss-hap");

        rv = mInfoArray->RemoveElement(rootEntry); // update the root array
        if (NS_FAILED(rv)) return rv;

        NS_RELEASE(rootEntry);
    }
    
    NS_RELEASE(info);

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEService::EnumerateEntries(nsIBidirectionalEnumerator* *aEnumerator) {
    return NS_NewISupportsArrayEnumerator(mInfoArray, aEnumerator);
}

NS_IMETHODIMP
nsMIMEService::Serialize() {
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsMIMEService::InitFromHack() {
    nsresult rv;

    rv = AddMapping(TEXT_PLAIN, "txt", "Text File", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_PLAIN, "text");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(APPLICATION_OCTET_STREAM, "exe", "Binary Executable", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "bin");
    if (NS_FAILED(rv)) return rv;
#if defined(VMS)
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "sav");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "bck");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "pcsi");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "pcsi-dcx_axpexe");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "pcsi-dcx_vaxexe");
    if (NS_FAILED(rv)) return rv;
#endif

    rv = AddMapping(TEXT_HTML, "htm", "Hyper Text Markup Language", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_HTML, "html");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_HTML, "shtml");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_HTML, "ehtml");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_RDF, "rdf", "Resource Description Framework", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_XUL, "xul", "XML-Based User Interface Language", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_XML, "xml", "Extensible Markup Language", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_CSS, "css", "Style Sheet", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(APPLICATION_JAVASCRIPT, "js", "Javascript Source File", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(MESSAGE_RFC822, "eml", "RFC-822 data", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_GIF, "gif", "GIF Image", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_JPG, "jpeg", "JPEG Image", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(IMAGE_JPG, "jpg");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_PNG, "png", "PNG Image", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_ART, "art", "ART Image", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_TIFF, "tiff", "TIFF Image", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(IMAGE_TIFF, "tif");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(APPLICATION_POSTSCRIPT, "ps", "Postscript File", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_POSTSCRIPT, "eps");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_POSTSCRIPT, "ai");
    if (NS_FAILED(rv)) return rv;
                 
    rv = AddMapping(TEXT_RTF, "rtf", "Rich Text Format", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_RTF, "rtf");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_CPP, "cpp", "CPP file", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_CPP, "cpp");
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

    nsAutoString specStr; specStr.AssignWithConversion(cStrSpec);

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
