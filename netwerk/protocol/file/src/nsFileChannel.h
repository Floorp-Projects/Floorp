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

#include "nsIFileChannel.h"

class nsIEventSinkGetter;

class nsFileChannel : public nsIFileChannel {
public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIChannel:

    /* readonly attribute nsIURI URI; */
    NS_IMETHOD GetURI(nsIURI * *aURI);

    /* nsIInputStream OpenInputStream (); */
    NS_IMETHOD OpenInputStream(PRUint32 startPosition, PRInt32 count, nsIInputStream **_retval);

    /* nsIOutputStream OpenOutputStream (); */
    NS_IMETHOD OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval);

    /* void AsyncRead (in unsigned long startPosition, in long readCount, in nsISupports ctxt, in nsIEventQueue eventQueue, in nsIStreamListener listener); */
    NS_IMETHOD AsyncRead(PRUint32 startPosition, PRInt32 readCount, nsISupports *ctxt, nsIEventQueue *eventQueue, nsIStreamListener *listener);

    /* void AsyncWrite (in nsIInputStream fromStream, in unsigned long startPosition, in long writeCount, in nsISupports ctxt, in nsIEventQueue eventQueue, in nsIStreamObserver observer); */
    NS_IMETHOD AsyncWrite(nsIInputStream *fromStream, PRUint32 startPosition, PRInt32 writeCount, nsISupports *ctxt, nsIEventQueue *eventQueue, nsIStreamObserver *observer);

    /* void Cancel (); */
    NS_IMETHOD Cancel();

    /* void Suspend (); */
    NS_IMETHOD Suspend();

    /* void Resume (); */
    NS_IMETHOD Resume();

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

    ////////////////////////////////////////////////////////////////////////////
    // nsFileChannel:

    nsFileChannel();
    // Always make the destructor virtual: 
    virtual ~nsFileChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsIURI* uri, nsIEventSinkGetter* getter, nsIEventQueue* queue);

protected:
};

#endif // nsFileChannel_h__
