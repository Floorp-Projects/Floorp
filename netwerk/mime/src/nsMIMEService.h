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

/*
 * There is a 1-to-many relationship between MIME-types and file extensions
 * I.e. there can be many file extensions that have the same mime type.
 */

#ifndef ___nsIMIMEService__h___
#define ___nsIMIMEService__h___

#include "nsIMIMEService.h"
#include "nsIURI.h"
#include "nsHashtable.h"


class nsMIMEService : public nsIMIMEService {

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsMIMEService methods
    nsMIMEService();
    virtual ~nsMIMEService();

    // nsIMIMEService methods
    NS_IMETHOD GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval);
    NS_IMETHOD GetTypeFromExtension(const char *aFileExt, char **aContentType);
    NS_IMETHOD GetTypeFromURI(nsIURI *aURI, char **aContentType);
    NS_IMETHOD GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval);

    NS_IMETHOD AddMIMEInfo(nsIMIMEInfo *aMIMEInfo);
    NS_IMETHOD RemoveMIMEInfo(nsIMIMEInfo *aMIMEInfo);

private:
    nsresult AddMapping(const char* mimeType, 
                        const char* extension,
                        const char* description);
    nsresult InitFromURI(nsIURI *aUri);
    nsresult InitFromFile(const char *aFileName);
    nsresult InitFromHack();

    
    nsHashtable             *mInfoHashtable;
};

#endif // ___nsIMIMEService__h___
