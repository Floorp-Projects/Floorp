/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __nsstreamconverterservice__h___
#define __nsstreamconverterservice__h___

#include "nsIStreamConverterService.h"
#include "nsIStreamListener.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"

class nsStreamConverterService : public nsIStreamConverterService {
public:    
    /////////////////////////////////////////////////////
    // nsISupports methods
    NS_DECL_ISUPPORTS


    /////////////////////////////////////////////////////
    // nsIStreamConverterService methods
    NS_IMETHOD RegisterConverter(const char *aProgID, nsISupports *aConverter);


    /////////////////////////////////////////////////////
    // nsIStreamConverter methods
    NS_IMETHOD Convert(nsIInputStream *aFromStream, const PRUnichar *aFromType, const PRUnichar *aToType, nsIInputStream **_retval)
    { return NS_ERROR_NOT_IMPLEMENTED;};
    NS_IMETHOD AsyncConvertStream(nsIInputStream *aFromStream, const PRUnichar *aFromType, const PRUnichar *aToType, nsIStreamListener *aListener)
    { return NS_ERROR_NOT_IMPLEMENTED;};
    NS_IMETHOD AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType, nsIStreamListener *aListener)
    { return NS_ERROR_NOT_IMPLEMENTED;};


    /////////////////////////////////////////////////////
    // nsIStreamListener methods
    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count);


    /////////////////////////////////////////////////////
    // nsIStreamObserver methods
    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt);
    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);

    /////////////////////////////////////////////////////
    // nsStreamConverterService methods
    nsStreamConverterService();
    virtual ~nsStreamConverterService();

    // Initialization routine. Must be called after this object is constructed.
    nsresult Init();

private:
    // Responsible for finding a converter for the given MIME-type.
    nsresult FindConverter(const char *aProgID, PRBool direct, nsCID *aCID, nsVoidArray **aEdgeList);

    // member variables
    nsHashtable              *mAdjacencyList;
};

#endif // __nsstreamconverterservice__h___