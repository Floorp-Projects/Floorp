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
                      public nsIBufferObserver
{
public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIRequest:

    /* boolean IsPending (); */
    NS_IMETHOD IsPending(PRBool *result);
    
    /* void Cancel (); */
    NS_IMETHOD Cancel();

    /* void Suspend (); */
    NS_IMETHOD Suspend();

    /* void Resume (); */
    NS_IMETHOD Resume();

    ////////////////////////////////////////////////////////////////////////////
    // from nsIChannel:

    /* readonly attribute nsIURI URI; */
    NS_IMETHOD GetURI(nsIURI * *aURI);

    /* nsIInputStream OpenInputStream (); */
    NS_IMETHOD OpenInputStream(PRUint32 startPosition, PRInt32 count, nsIInputStream **_retval);

    /* nsIOutputStream OpenOutputStream (); */
    NS_IMETHOD OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval);

    /* void AsyncRead (in unsigned long startPosition, in long readCount, in nsISupports ctxt, in nsIStreamListener listener); */
    NS_IMETHOD AsyncRead(PRUint32 startPosition, PRInt32 readCount, nsISupports *ctxt, nsIStreamListener *listener);

    /* void AsyncWrite (in nsIInputStream fromStream, in unsigned long startPosition, in long writeCount, in nsISupports ctxt, in nsIStreamObserver observer); */
    NS_IMETHOD AsyncWrite(nsIInputStream *fromStream, PRUint32 startPosition, PRInt32 writeCount, nsISupports *ctxt, nsIStreamObserver *observer);

    /* attribute boolean LoadQuiet; */
    NS_IMETHOD GetLoadAttributes(PRUint32 *aLoadAttributes);
    NS_IMETHOD SetLoadAttributes(PRUint32 aLoadAttributes);

    /* readonly attribute string ContentType; */
    NS_IMETHOD GetContentType(char * *aContentType);

    NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup);
    NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIFileChannel:

    /* readonly attribute PRTime CreationDate; */
    NS_IMETHOD GetCreationDate(PRTime *aCreationDate);

    /* readonly attribute PRTime ModDate; */
    NS_IMETHOD GetModDate(PRTime *aModDate);

    /* readonly attribute unsigned long FileSize; */
    NS_IMETHOD GetFileSize(PRUint32 *aFileSize);

    /* readonly attribute nsIFileChannel Parent; */
    NS_IMETHOD GetParent(nsIFileChannel * *aParent);

    /* readonly attribute nsISimpleEnumerator Children; */
    NS_IMETHOD GetChildren(nsISimpleEnumerator * *aChildren);

    /* readonly attribute string NativePath; */
    NS_IMETHOD GetNativePath(char * *aNativePath);

    /* boolean Exists (); */
    NS_IMETHOD Exists(PRBool *_retval);

    /* void Create (); */
    NS_IMETHOD Create();

    /* void Delete (); */
    NS_IMETHOD Delete();

    /* void MoveFrom (in nsIURI src); */
    NS_IMETHOD MoveFrom(nsIURI *src);

    /* void CopyFrom (in nsIURI src); */
    NS_IMETHOD CopyFrom(nsIURI *src);

    /* boolean IsDirectory (); */
    NS_IMETHOD IsDirectory(PRBool *_retval);

    /* boolean IsFile (); */
    NS_IMETHOD IsFile(PRBool *_retval);

    /* boolean IsLink (); */
    NS_IMETHOD IsLink(PRBool *_retval);

    /* nsIFileChannel ResolveLink (); */
    NS_IMETHOD ResolveLink(nsIFileChannel **_retval);

    /* string MakeUniqueFileName (in string baseName); */
    NS_IMETHOD MakeUniqueFileName(const char *baseName, char **_retval);

    /* void Execute (in string args); */
    NS_IMETHOD Execute(const char *args);

    ////////////////////////////////////////////////////////////////////////////
    // nsIRunnable methods:

    NS_IMETHOD Run(void);

    ////////////////////////////////////////////////////////////////////////////
    // nsIBufferObserver:

    NS_IMETHOD OnFull(nsIBuffer* buffer);

    NS_IMETHOD OnWrite(nsIBuffer* aBuffer, PRUint32 aCount);

    NS_IMETHOD OnEmpty(nsIBuffer* buffer);

    ////////////////////////////////////////////////////////////////////////////
    // nsFileChannel:

    nsFileChannel();
    // Always make the destructor virtual: 
    virtual ~nsFileChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsFileProtocolHandler* handler,
                  const char* verb, nsIURI* uri, nsIEventSinkGetter* getter,
                  nsIEventQueue* queue);

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

#ifdef STREAM_CONVERTER_HACK
	nsCOMPtr<nsIStreamConverter2> mStreamConverter;
	nsXPIDLCString				 mStreamConverterOutType;
#endif
};

#define NS_FILE_TRANSPORT_SEGMENT_SIZE   (4*1024)
#define NS_FILE_TRANSPORT_BUFFER_SIZE    (1024*1024)//(32*1024)

#endif // nsFileChannel_h__
