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

#ifndef nsPluginInputStream_h__
#define nsPluginInputStream_h__

#include "net.h"
#include "nppriv.h"

#ifdef NEW_PLUGIN_STREAM_API

#include "nsIPluginInputStream2.h"
#include "nsIPluginStreamListener.h"

class nsPluginInputStream;

// stored in the fe_data of the URL_Struct:
struct nsPluginURLData {
    struct _NPEmbeddedApp* app;
    nsIPluginStreamListener* listener;
    nsPluginInputStream* inStr;
};

class nsPluginInputStream : public nsIPluginInputStream2 {
public:

    NS_DECL_ISUPPORTS

    NS_DECL_NSIBASESTREAM

    NS_DECL_NSIINPUTSTREAM

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInputStream:

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD
    GetLastModified(PRUint32 *result);

    NS_IMETHOD
    RequestRead(nsByteRange* rangeList);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInputStream2:

    NS_IMETHOD
    GetContentLength(PRUint32 *result);

    NS_IMETHOD
    GetHeaderFields(PRUint16& n, const char*const*& names, const char*const*& values);

    NS_IMETHOD
    GetHeaderField(const char* name, const char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginInputStream specific methods:

    nsPluginInputStream(nsIPluginStreamListener* listener,
                        nsPluginStreamType streamType);
    virtual ~nsPluginInputStream(void);

    nsIPluginStreamListener* GetListener(void) { return mListener; }
    nsPluginStreamType GetStreamType(void) { return mStreamType; }
    PRBool IsClosed(void) { return mClosed; }

    void SetStreamInfo(URL_Struct* urls, np_stream* stream) {
        mUrls = urls;
        mStream = stream;
    }

    nsresult ReceiveData(const char* buffer, PRUint32 offset, PRUint32 len);
    void Cleanup(void);

protected:
    nsIPluginStreamListener* mListener;
    nsPluginStreamType mStreamType;
    URL_Struct* mUrls;
    np_stream* mStream;

    struct BufferElement {
        BufferElement* next;
        char* segment;
        PRUint32 offset;
        PRUint32 length;
    };

    BufferElement* mBuffer;
    PRBool mClosed;
//    PRUint32 mReadCursor;

//    PRUint32 mBufferLength;
//    PRUint32 mAmountRead;

};

#else // !NEW_PLUGIN_STREAM_API

class nsPluginStreamPeer : public nsIPluginStreamPeer2, 
                           public nsISeekablePluginStreamPeer
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamPeer:

    // (Corresponds to NPStream's url field.)
    NS_IMETHOD
    GetURL(const char* *result);

    // (Corresponds to NPStream's end field.)
    NS_IMETHOD
    GetEnd(PRUint32 *result);

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD
    GetLastModified(PRUint32 *result);

    // (Corresponds to NPStream's notifyData field.)
    NS_IMETHOD
    GetNotifyData(void* *result);

    // (Corresponds to NPP_DestroyStream's reason argument.)
    NS_IMETHOD
    GetReason(nsPluginReason *result);

    // (Corresponds to NPP_NewStream's MIMEType argument.)
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamPeer2:

    NS_IMETHOD
    GetContentLength(PRUint32 *result);

    NS_IMETHOD
    GetHeaderFieldCount(PRUint32 *result);

    NS_IMETHOD
    GetHeaderFieldKey(PRUint32 index, const char* *result);

    NS_IMETHOD
    GetHeaderField(PRUint32 index, const char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsISeekablePluginStreamPeer:

    // (Corresponds to NPN_RequestRead.)
    NS_IMETHOD
    RequestRead(nsByteRange* rangeList);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginStreamPeer specific methods:
    
    nsPluginStreamPeer(URL_Struct *urls, np_stream *stream);
    virtual ~nsPluginStreamPeer(void);

    NS_DECL_ISUPPORTS
    
    nsIPluginStream* GetUserStream(void) {
        return userStream;
    }

    void SetUserStream(nsIPluginStream* str) {
        userStream = str;
    }

    void SetReason(nsPluginReason r) {
        reason = r;
    }

protected:
    nsIPluginStream* userStream;
    URL_Struct *urls;
    np_stream *stream;
    nsPluginReason reason;

};

#endif // !NEW_PLUGIN_STREAM_API

#endif // nsPluginInputStream_h__
