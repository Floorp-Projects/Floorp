/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


// nsISupports methods
NS_IMPL_ISUPPORTS(nsMIMEService, nsCOMTypeInfo<nsIMIMEService>::GetIID());


// nsMIMEService methods
nsMIMEService::nsMIMEService() {

    NS_INIT_REFCNT();

    mInfoArray = new nsVoidArray();
    InitFromHack(); // XXX bogus
}

nsMIMEService::~nsMIMEService() {
    PRInt32 count = mInfoArray->Count();
    for (int i = 0; i < count; i++) {
        nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoArray->ElementAt(i);
        mInfoArray->RemoveElementAt(i);
        //    delete entry;
    }
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
nsMIMEService::InitFromHack() {

    nsMIMEInfoImpl* anInfo = nsnull;
    anInfo = new nsMIMEInfoImpl("text/html", "htm,html", "Hyper Text Markup Language");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("text/rdf", "rdf", "Resource Description Framework");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("text/xul", "xul", "XML-Based User Interface Language");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("text/xml", "xml", "Extensible Markup Language");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("text/css", "css", "Style Sheet");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("application/x-javascript", "js", "Javascript Source File");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

	 anInfo = new nsMIMEInfoImpl("message/rfc822", "eml", "RFC-822 data");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    /////////////////
    // Images
    /////////////////

    anInfo = new nsMIMEInfoImpl("image/gif", "gif", "GIF Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("image/jpeg", "jpeg,jpg,jpe,jfif,pjpeg,pjp", "JPEG Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("image/png", "png", "PNG Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);

    anInfo = new nsMIMEInfoImpl("image/tiff", "tiff,tif", "TIFF Image");
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;
    mInfoArray->AppendElement(anInfo);
    
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
    
    return NS_OK;
}


// nsIMIMEService methods
NS_IMETHODIMP
nsMIMEService::GetFromExtension(const PRUnichar *aFileExt, nsIMIMEInfo **_retval) {
    nsIAtom* extension = NS_NewAtom(aFileExt);
    if (!extension) return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 count = mInfoArray->Count();
    for (int i = 0; i < count; i++) {
        nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoArray->ElementAt(i);
        if (entry->InExtensions(extension)) {
            *_retval = NS_STATIC_CAST(nsIMIMEInfo*, entry);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMIMEService::GetFromMIMEType(const PRUnichar *aMIMEType, nsIMIMEInfo **_retval) {
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
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMIMEService::AddMIMEInfo(nsIMIMEInfo *aMIMEInfo) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEService::RemoveMIMEInfo(nsIMIMEInfo *aMIMEInfo) {
    return NS_ERROR_NOT_IMPLEMENTED;
}
