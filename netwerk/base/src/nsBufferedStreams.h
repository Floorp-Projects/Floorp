/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s): 
 */

#ifndef nsBufferedStreams_h__
#define nsBufferedStreams_h__

#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

////////////////////////////////////////////////////////////////////////////////

class nsBufferedStream : public nsIBaseStream, 
                         public nsIRandomAccessStore
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIBASESTREAM
    NS_DECL_NSIRANDOMACCESSSTORE

    nsBufferedStream();
    virtual ~nsBufferedStream();

    nsresult Init(PRUint32 bufferSize, nsIBaseStream* stream);

protected:
    PRUint32                    mBufferSize;
    char*                       mBuffer;
    PRUint32                    mBufferStartOffset;
    PRUint32                    mCursor;
    nsCOMPtr<nsIBaseStream>     mStream;
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedInputStream : public nsBufferedStream,
                              public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIBASESTREAM
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIRANDOMACCESSSTORE

    nsBufferedInputStream() : nsBufferedStream() {}
    virtual ~nsBufferedInputStream() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIInputStream* Source() { 
        return (nsIInputStream*)mStream.get();
    }
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedOutputStream : public nsBufferedStream, 
                               public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIBASESTREAM
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIRANDOMACCESSSTORE

    nsBufferedOutputStream() : nsBufferedStream() {}
    virtual ~nsBufferedOutputStream() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIOutputStream* Sink() { 
        return (nsIOutputStream*)mStream.get();
    }
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsBufferedStreams_h__
