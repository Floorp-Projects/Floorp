/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsMemoryCacheTransport.h, released February 26, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsMemoryCacheTransport_h__
#define nsMemoryCacheTransport_h__

#include "nsISupports.h"
#include "nsITransport.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "prclist.h"
#include "nsCOMPtr.h"
#include "nsIStreamListener.h"

#define NS_MEMORY_CACHE_SEGMENT_SIZE  1024
#define NS_MEMORY_CACHE_BUFFER_SIZE   1024 * 1024

class nsMemoryCacheIS;  // non-blocking input stream
class nsMemoryCacheBS;  // blocking stream base class
class nsMemoryCacheBIS; // blocking input stream
class nsMemoryCacheBOS; // blocking output stream

/**
 * Each "stream-based" memory cache entry has one transport
 * associated with it.  The transport supports multiple 
 * simultaneous read requests and a single write request.
 */
class nsMemoryCacheTransport : public nsITransport
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT

    nsMemoryCacheTransport();
    virtual ~nsMemoryCacheTransport();

    nsresult Init(PRUint32 aBufSegmentSize, PRUint32 aBufMaxSize);

    /* private */

    nsresult GetReadSegment(PRUint32 aOffset, char **aPtr, PRUint32 *aCount); 

    nsresult GetWriteSegment(char **aPtr, PRUint32 *aCount);
    nsresult AddToBytesWritten(PRUint32 aCount);

    nsresult CloseOutputStream();

private:

    struct nsSegment
    {
        struct nsSegment *next;
          // segments are kept in a singly linked list
        
        char *Data() { return NS_REINTERPRET_CAST(char *, this) + sizeof(*this); }
          // the data follows this structure (the two are allocated together)
    };

    nsresult AddWriteSegment();
    void AppendSegment(nsSegment *);
    void DeleteSegments();

private:
    nsMemoryCacheBOS  *mOutputStream; // weak ref
    PRCList           *mInputStreams; // weak ref
    PRCList           *mReadRequests; // weak ref

    PRUint32           mSegmentSize;
    PRUint32           mMaxSize;
    PRUint32           mWriteCursor;
    nsSegment         *mSegments;
    nsSegment         *mWriteSegment;
};

/**
 * The transport request object returned by AsyncRead
 */
class nsMemoryCacheReadRequest : public PRCList
                               , public nsITransportRequest
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORTREQUEST
    NS_DECL_NSIREQUEST

    nsMemoryCacheReadRequest();
    virtual ~nsMemoryCacheReadRequest();

    void SetTransport(nsMemoryCacheTransport *t) { mTransport = t; }

private:
    nsMemoryCacheTransport     *mTransport;   // weak ref
    nsMemoryCacheIS            *mInputStream; // strong ref
    PRUint32                    mOffset;
    PRUint32                    mCountRemaining;
    nsresult                    mStatus;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports>       mListenerContext;
    PRPackedBool                mCanceled;
};

/**
 * The non-blocking input stream passed through OnDataAvailable
 */
class nsMemoryCacheIS : public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

    nsMemoryCacheIS();
    virtual ~nsMemoryCacheIS();

private:
    nsMemoryCacheReadRequest *mRequest; // weak ref
};

/**
 * A base class for the blocking streams
 */
class nsMemoryCacheBS
{
public:
    nsMemoryCacheBS();
    virtual ~nsMemoryCacheBS();

    void SetTransport(nsMemoryCacheTransport *t) { mTransport = t; }

protected:
    nsMemoryCacheTransport *mTransport; // weak ref
    PRUint32                mCountRemaining;
};

/**
 * The blocking input stream object returned by OpenInputStream
 */
class nsMemoryCacheBIS : public PRCList
                       , public nsMemoryCacheBS
                       , public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

    nsMemoryCacheBIS();
    virtual ~nsMemoryCacheBIS();

private:
    PRUint32 mOffset;
};

/**
 * The blocking output stream object returned by OpenOutputStream
 */
class nsMemoryCacheBOS : public nsMemoryCacheBS
                       , public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM

    nsMemoryCacheBOS();
    virtual ~nsMemoryCacheBOS();
};

#endif
