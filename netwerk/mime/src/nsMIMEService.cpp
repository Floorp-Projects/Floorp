/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsMIMEService.h"
#include "nsVoidArray.h"
#include "nsEnumeratorUtils.h" // for nsArrayEnumerator
#include "nsString2.h"
#include "nsMIMEInfoImpl.h"
#include "nsIMIMEInfo.h"
#include "nsIFileSpec.h"
#include "nsIURL.h"

PRBool DeleteEntry(nsHashKey *aKey, void *aData, void* closure) {
    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)aData;
    delete entry;
    entry = nsnull;
    return PR_TRUE;   
};

PRBool FindMIMEType(nsHashKey *aKey, void *aData, void* closure) {
    return PR_FALSE;
}


// nsISupports methods
NS_IMPL_ISUPPORTS(nsMIMEService, nsCOMTypeInfo<nsIMIMEService>::GetIID());


// nsMIMEService methods
nsMIMEService::nsMIMEService() {

    NS_INIT_REFCNT();

    mInfoHashtable = new nsHashtable();
    InitFromHack(); // XXX bogus
    //nsresult rv = InitializeMIMEMap(mInfoHashtable);
}

nsMIMEService::~nsMIMEService() {
    mInfoHashtable->Enumerate(DeleteEntry, nsnull);
    mInfoHashtable->Reset();
    delete mInfoHashtable;
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

nsresult
nsMIMEService::AddMapping(const char* mimeType, 
                          const char* extension,
                          const char* description)
{
    nsMIMEInfoImpl* anInfo = nsnull;
    nsStringKey *extKey = nsnull;

    anInfo = new nsMIMEInfoImpl(mimeType, extension, description);
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(anInfo);
    extKey = new nsStringKey(extension);
    if (!extKey) return NS_ERROR_OUT_OF_MEMORY;
    mInfoHashtable->Put(extKey, anInfo);
    delete extKey;
    return NS_OK;
}

nsresult
nsMIMEService::InitFromHack() {
    nsresult rv;
    rv = AddMapping("text/html", "htm", "Hyper Text Markup Language");
    if (NS_FAILED(rv)) return rv;
    rv = AddMapping("text/html", "html", "Hyper Text Markup Language");
    if (NS_FAILED(rv)) return rv;
    rv = AddMapping("text/html", "shtml", "Hyper Text Markup Language");
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
    rv = AddMapping("image/jpeg", "jpg", "JPEG Image");
    if (NS_FAILED(rv)) return rv;
    rv = AddMapping("image/png", "png", "PNG Image");
    if (NS_FAILED(rv)) return rv;
    rv = AddMapping("image/tiff", "tiff", "TIFF Image");
    if (NS_FAILED(rv)) return rv;
    rv = AddMapping("image/tiff", "tif", "TIFF Image");
    if (NS_FAILED(rv)) return rv;
#if 0
    anInfo = new nsMIMEInfoImpl("image/x-cmu-raster", "ras", "CMU Raster Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-xbitmap", "xbm", "X Bitmap");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-xpixmap", "xpm", "X Pixmap");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-xwindowdump", "xwd", "X Window Dump Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-portable-anymap", "pnm", "PBM Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-portable-bitmap", "pbm", "PBM Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-portable-graymap", "pgm", "PGM Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-portable-pixmap", "ppm", "PPM Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-rgb", "rgb", "RGB Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
    anInfo = new nsMIMEInfoImpl("image/x-MS-bmp", "bmp", "Windows Bitmap");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
     anInfo = new nsMIMEInfoImpl("image/x-photo-cd", "pcd", "PhotoCD Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
     anInfo = new nsMIMEInfoImpl("image/ief", "ief", "");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
     anInfo = new nsMIMEInfoImpl("application/fractals", "fif", "Fractal Image Format");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
#endif // 0
    
    return NS_OK;
}


// nsIMIMEService methods
NS_IMETHODIMP
nsMIMEService::GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval) {
    nsStringKey key(aFileExt);

    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoHashtable->Get(&key);
    if (!entry) return NS_ERROR_FAILURE;
    NS_ADDREF(entry);
    *_retval = NS_STATIC_CAST(nsIMIMEInfo*, entry);
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEService::GetTypeFromExtension(const char *aFileExt, char **aContentType) {
    nsresult rv;
    nsIMIMEInfo *info = nsnull;
    rv = GetFromExtension(aFileExt, &info);
    if (NS_FAILED(rv)) return rv;

    rv = info->GetMIMEType(aContentType);
    NS_RELEASE(info);
    return rv;
}

NS_IMETHODIMP
nsMIMEService::GetTypeFromURI(nsIURI *aURI, char **aContentType) {
    nsresult rv = NS_ERROR_FAILURE;
    char *cStrSpec= nsnull;
    nsString2 specStr;

    // first try to get a url out of the uri so we can skip post
    // filename stuff (i.e. query string)
    nsIURL *url = nsnull;
    rv = aURI->QueryInterface(nsCOMTypeInfo<nsIURL>::GetIID(), (void**)&url);
    if (NS_SUCCEEDED(rv)) {
        rv = url->GetFileName(&cStrSpec);
        NS_RELEASE(url);
        if (NS_FAILED(rv)) return rv;
    } else {
        // no url, let's give the raw spec a shot
        rv = aURI->GetSpec(&cStrSpec);
        if (NS_FAILED(rv)) return rv;
    }

    // find the file extension (if any)
    specStr.SetString(cStrSpec);
    nsString2 extStr;
    PRInt32 extLoc = specStr.RFindChar('.');
    if (-1 != extLoc) {
        specStr.Right(extStr, specStr.Length() - extLoc - 1);
        char *ext = extStr.ToNewCString();
        if (!ext) return NS_ERROR_OUT_OF_MEMORY;
        rv = GetTypeFromExtension(ext, aContentType);
        nsAllocator::Free(ext);
    }
    return rv;
}

NS_IMETHODIMP
nsMIMEService::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval) {

#if 0
    // hashtable, here's it's a little trickier because we have to enumerate the hashtable

    nsIAtom* MIMEType = NS_NewAtom(aMIMEType);
    if (!MIMEType) return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 count = mInfoArray->Count();
    for (int i = 0; i < count; i++) {
        nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoArray->ElementAt(i);
        if (entry->mMIMEType == MIMEType) {
            *_retval = NS_STATIC_CAST(nsIMIMEInfo*, entry);
            return NS_OK;
        }
    }
#endif
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEService::AddMIMEInfo(nsIMIMEInfo *aMIMEInfo) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEService::RemoveMIMEInfo(nsIMIMEInfo *aMIMEInfo) {
    return NS_ERROR_NOT_IMPLEMENTED;
}
