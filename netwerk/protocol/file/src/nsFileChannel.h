/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#ifndef nsFileChannel_h__
#define nsFileChannel_h__

// mscott --  this is just one temporary hack until we have a legit stream converter
// story going....if the file we are opening is an rfc822 file then we want to 
// go out and convert the data into html before we try to load it. so I'm inserting
// code which if we are rfc-822 will cause us to literally insert a converter between
// the file channel stream of incoming data and the consumer at the other end of the
// AsyncRead call...

#define STREAM_CONVERTER_HACK 

#include "nsIFileChannel.h"
#include "nsIThread.h"
#include "nsFileSpec.h"
#include "prlock.h"
#include "nsIEventQueueService.h"
#include "nsIBuffer.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsCOMPtr.h"

#ifdef STREAM_CONVERTER_HACK
#include "nsIStreamConverter2.h"
#include "nsXPIDLString.h"
#endif

class nsIEventSinkGetter;
class nsIStreamListener;
class nsFileProtocolHandler;
class nsIBaseStream;
class nsIBuffer;
class nsIBufferInputStream;
class nsIBufferOutputStream;

class nsFileChannel : public nsIFileChannel, 
                      public nsIRunnable,
                      public nsIBufferObserver,
                      public nsIStreamListener
{
public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIRequest:
    NS_DECL_NSIREQUEST

    ////////////////////////////////////////////////////////////////////////////
    // from nsIChannel:
    NS_DECL_NSICHANNEL

    ////////////////////////////////////////////////////////////////////////////
    // from nsIFileChannel:
    NS_DECL_NSIFILECHANNEL

    ////////////////////////////////////////////////////////////////////////////
    // nsIRunnable methods:

    NS_IMETHOD Run(void);

    ////////////////////////////////////////////////////////////////////////////
    // nsIBufferObserver:

    NS_IMETHOD OnFull(nsIBuffer* buffer);

    NS_IMETHOD OnWrite(nsIBuffer* aBuffer, PRUint32 aCount);

    NS_IMETHOD OnEmpty(nsIBuffer* buffer);

    ////////////////////////////////////////////////////////////////////////////
    // nsIStreamListener:

    NS_IMETHOD OnDataAvailable(nsIChannel* channel, nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength);


    NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports* context);

    NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                            nsresult aStatus,
                            const PRUnichar* aMsg);

    ////////////////////////////////////////////////////////////////////////////
    // nsFileChannel:

    nsFileChannel();
    // Always make the destructor virtual: 
    virtual ~nsFileChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsFileProtocolHandler* handler,
                  const char* verb, nsIURI* uri, nsILoadGroup *aGroup,
                  nsIEventSinkGetter* getter);

    void Process(void);

    enum State {
        QUIESCENT,
        START_READ,
        READING,
        START_WRITE,
        WRITING,
        ENDING
    };

protected:
    nsresult CreateFileChannelFromFileSpec(nsFileSpec& spec, nsIFileChannel** result);

protected:
    nsIURI*                     mURI;
    nsIEventSinkGetter*         mGetter;        // XXX it seems wrong keeping this -- used by GetParent
    nsIStreamListener*          mListener;
    nsIEventQueue*              mEventQueue;

    nsFileSpec                  mSpec;

    nsISupports*                mContext;
    nsFileProtocolHandler*      mHandler;
    State                       mState;
    PRBool                      mSuspended;

    // state variables:
    nsIBaseStream*              mFileStream;    // cast to nsIInputStream/nsIOutputStream for reading/Writing
    nsIBufferInputStream*       mBufferInputStream;
    nsIBufferOutputStream*      mBufferOutputStream;
    nsresult                    mStatus;
    PRUint32                    mSourceOffset;
    PRUint32                    mAmount;
	PRBool						mReadFixedAmount;  // if user wants to only read a fixed number of bytes set this flag

    PRMonitor*                  mMonitor;
    PRUint32                    mLoadAttributes;
    nsILoadGroup*               mLoadGroup;

    nsCOMPtr<nsIStreamListener> mRealListener;

#ifdef STREAM_CONVERTER_HACK
	nsCOMPtr<nsIStreamConverter2> mStreamConverter;
	nsXPIDLCString				 mStreamConverterOutType;
#endif
};

#define NS_FILE_TRANSPORT_SEGMENT_SIZE   (4*1024)
#define NS_FILE_TRANSPORT_BUFFER_SIZE    (32*1024)

#endif // nsFileChannel_h__
