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

/*
 * There is a 1-to-many relationship between MIME-types and file extensions
 * I.e. there can be many file extensions that have the same mime type.
 */

#ifndef ___nsXMLMIMEDataSource__h___
#define ___nsXMLMIMEDataSource__h___

#include "nsIMIMEDataSource.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsHashtable.h"
#include "nsISupportsArray.h"
#include "nsIFile.h"
class nsIFile;

class nsXMLMIMEDataSource : public nsIMIMEDataSource {

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMIMEDATASOURCE

    // nsMIMEService methods
    nsXMLMIMEDataSource();
    virtual ~nsXMLMIMEDataSource();
    nsresult Init();
    static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);

private:
	nsresult AddMapping(const char* mimeType, 
                          const char* extension,
                          const char* description,
                          nsIURI* dataURI, PRUint32 type=PRUint32(0x3F3F3F3F), PRUint32 creator=PRUint32(0x3F3F3F3F));
	nsresult AppendExtension(const char* mimeType, const char* extension);
	nsresult RemoveExtension(const char* aExtension);

    nsresult InitFromURI(nsIURI *aUri);
    nsresult InitFromHack();
    nsresult InitFromFile( nsIFile* /* aFile */ );

    nsCOMPtr<nsIFile>						mFile; // File to load the datasource from
    nsHashtable                 *mInfoObjects; // used for fast access and
                                               // multi index lookups
		
		nsHashtable									mMacCache; // Cache for lookup of file creator types
						
    nsCOMPtr<nsISupportsArray>   mInfoArray;   // used for enumeration
};

#endif // ___nsXMLMIMEDataSource__h___
