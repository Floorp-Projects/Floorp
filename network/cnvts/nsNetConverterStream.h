/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsNetConverterStream_h___
#define nsNetConverterStream_h___

#include "nsINetOStream.h"


class nsNetConverterStream : public nsINetOStream {
public:
    nsNetConverterStream();
    virtual ~nsNetConverterStream();

    NS_DECL_ISUPPORTS


    //nsINetOStream interface

    NS_IMETHOD
    Complete(void);
    
    NS_IMETHOD
    Abort(PRInt32 status);

    NS_IMETHOD
    WriteReady(PRUint32 *aReadyCount);


    // nsIOutputStream interface

    NS_IMETHOD Write(const char *aBuf,
                     PRUint32 aLen,
                     PRUint32 *aWriteLength);

    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *aWriteCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    } 

    NS_IMETHOD Flush(void) {
        return NS_OK;
    }

    // nsIBaseStream interface

    NS_IMETHOD Close(void);


    //locals

    nsresult Initialize(void *next_stream);


private:
    void *mNextStream;

};

#endif /* nsNetConverterStream_h___ */
