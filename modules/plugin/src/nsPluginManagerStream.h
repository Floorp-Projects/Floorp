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

#ifndef nsPluginManagerStream_h__
#define nsPluginManagerStream_h__

#include "nsIOutputStream.h"
#include "npglue.h"

class nsPluginManagerStream : public nsIOutputStream {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIBaseStream:

    NS_IMETHOD
    Close(void);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIOutputStream:

    /** Write data into the stream.
     *  @param aBuf the buffer into which the data is read
     *  @param aOffset the start offset of the data
     *  @param aCount the maximum number of bytes to read
     *  @return number of bytes read or an error if < 0
     */   
    NS_IMETHOD
    Write(const char* aBuf, PRInt32 aCount, PRInt32 *resultingCount); 

    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *aWriteCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush() {
        return NS_OK;
    }

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginManagerStream specific methods:

    nsPluginManagerStream(NPP npp, NPStream* pstr);
    virtual ~nsPluginManagerStream(void);

    NS_DECL_ISUPPORTS

protected:
    NPP npp;
    NPStream* pstream;

};

#endif // nsPluginManagerStream_h__
