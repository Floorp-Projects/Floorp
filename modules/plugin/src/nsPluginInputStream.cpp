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

#include "nsPluginInputStream.h"
#include "plstr.h"
#include "npglue.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
#ifdef NEW_PLUGIN_STREAM_API
static NS_DEFINE_IID(kIPluginInputStreamIID, NS_IPLUGININPUTSTREAM_IID);
static NS_DEFINE_IID(kIPluginInputStream2IID, NS_IPLUGININPUTSTREAM2_IID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);
#else // !NEW_PLUGIN_STREAM_API
static NS_DEFINE_IID(kISeekablePluginStreamPeerIID, NS_ISEEKABLEPLUGINSTREAMPEER_IID);
static NS_DEFINE_IID(kIPluginStreamPeerIID, NS_IPLUGINSTREAMPEER_IID);
static NS_DEFINE_IID(kIPluginStreamPeer2IID, NS_IPLUGINSTREAMPEER2_IID);
#endif // !NEW_PLUGIN_STREAM_API

#ifdef NEW_PLUGIN_STREAM_API
////////////////////////////////////////////////////////////////////////////////
// Plugin Input Stream Interface

nsPluginInputStream::nsPluginInputStream(nsIPluginStreamListener* listener,
                                         nsPluginStreamType streamType)
    : mListener(listener), mStreamType(streamType),
      mUrls(NULL), mStream(NULL),
      mBuffer(NULL), mClosed(PR_FALSE)
//      mBuffer(NULL), mBufferLength(0), mAmountRead(0)
{
    NS_INIT_REFCNT();
    listener->AddRef();
}

nsPluginInputStream::~nsPluginInputStream(void)
{
    Cleanup();
    mListener->Release();
}

NS_IMPL_ADDREF(nsPluginInputStream);
NS_IMPL_RELEASE(nsPluginInputStream);

NS_METHOD
nsPluginInputStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(kIPluginInputStream2IID) ||
        aIID.Equals(kIPluginInputStreamIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (nsIPluginInputStream2*)this; 
        AddRef();
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
}

void 
nsPluginInputStream::Cleanup(void)
{
    if (mBuffer) {
        // free the buffered data
        BufferElement* element = mBuffer;
        while (element != NULL) {
            BufferElement* next = element->next;
            PL_strfree(element->segment);
            delete element;
            element = next;
        }
        mBuffer = NULL;
    }
    mClosed = PR_TRUE;
}

NS_METHOD
nsPluginInputStream::Close(void)
{
    NPError err = NPERR_NO_ERROR;
    Cleanup();
#if 0   /* According to the plugin documentation, this would seem to be the 
         * right thing to do here, but it's not (and calling NPN_DestroyStream
         * in the 4.0 browser during an NPP_Write call will crash the browser).
         */
    err = npn_destroystream(mStream->instance->npp, mStream->pstream, 
                            nsPluginReason_UserBreak);
#endif
    return fromNPError[err];
}

NS_METHOD
nsPluginInputStream::GetLength(PRInt32 *aLength)
{
    *aLength = mStream->pstream->end;
    return NS_OK;
#if 0
    *aLength = mBufferLength;
    return NS_OK;
#endif
}

nsresult
nsPluginInputStream::ReceiveData(const char* buffer, PRUint32 offset, PRUint32 len)
{
    BufferElement* element = new BufferElement;
    if (element == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    element->segment = PL_strdup(buffer);
    element->offset = offset;
    element->length = len;
    element->next = mBuffer;
    mBuffer = element;
    return NS_OK;
}

NS_METHOD
nsPluginInputStream::Read(char* aBuf, PRInt32 aOffset, PRInt32 aCount, 
                          PRInt32 *aReadCount)
{
    BufferElement* element;
    for (element = mBuffer; element != NULL; element = element->next) {
        if ((PRInt32)element->offset <= aOffset
            && aOffset < (PRInt32)(element->offset + element->length)) {
            // found our segment
            PRUint32 segmentIndex = aOffset - element->offset;
            PRUint32 segmentAmount = element->length - segmentIndex;
            if (aCount > (PRInt32)segmentAmount) {
                return NS_BASE_STREAM_EOF;      // XXX right error?
            }
            memcpy(aBuf, &element->segment[segmentIndex], aCount);
//            mReadCursor = segmentIndex + aCount;
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

NS_METHOD
nsPluginInputStream::GetLastModified(PRUint32 *result)
{
    *result = mStream->pstream->lastmodified;
    return NS_OK;
}

NS_METHOD
nsPluginInputStream::RequestRead(nsByteRange* rangeList)
{
    NPError err = npn_requestread(mStream->pstream,
                                  (NPByteRange*)rangeList);
    return fromNPError[err];
}

NS_METHOD
nsPluginInputStream::GetContentLength(PRUint32 *result)
{
    *result = mUrls->content_length;
    return NS_OK;
}

NS_METHOD
nsPluginInputStream::GetHeaderFields(PRUint16& n, const char*const*& names,
                                     const char*const*& values)
{
    n = (PRUint16)mUrls->all_headers.empty_index;
    names = (const char*const*)mUrls->all_headers.key;
    values = (const char*const*)mUrls->all_headers.value;
    return NS_OK;
}

NS_METHOD
nsPluginInputStream::GetHeaderField(const char* name, const char* *result)
{
    PRUint16 i;
    for (i = 0; i < mUrls->all_headers.empty_index; i++) {
        if (PL_strcmp(mUrls->all_headers.key[i], name) == 0) {
            *result = mUrls->all_headers.value[i];
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

#else // !NEW_PLUGIN_STREAM_API
////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Peer Interface

nsPluginStreamPeer::nsPluginStreamPeer(URL_Struct *urls, np_stream *stream)
    : userStream(NULL), urls(urls), stream(stream), 
      reason(nsPluginReason_NoReason)
{
    NS_INIT_REFCNT();
}

nsPluginStreamPeer::~nsPluginStreamPeer(void)
{
#if 0
    NPError err = npn_destroystream(stream->instance->npp, stream->pstream, reason);
    PR_ASSERT(err == nsPluginError_NoError);
#endif
}

NS_METHOD
nsPluginStreamPeer::GetURL(const char* *result)
{
    *result = stream->pstream->url;
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetEnd(PRUint32 *result)
{
    *result = stream->pstream->end;
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetLastModified(PRUint32 *result)
{
    *result = stream->pstream->lastmodified;
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetNotifyData(void* *result)
{
    *result = stream->pstream->notifyData;
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetReason(nsPluginReason *result)
{
    *result = reason;
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetMIMEType(nsMIMEType *result)
{
    *result = (nsMIMEType)urls->content_type;
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetContentLength(PRUint32 *result)
{
    *result = urls->content_length;
    return NS_OK;
}
#if 0
NS_METHOD_(const char*)
nsPluginStreamPeer::GetContentEncoding(void)
{
    return urls->content_encoding;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetCharSet(void)
{
    return urls->charset;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetBoundary(void)
{
    return urls->boundary;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetContentName(void)
{
    return urls->content_name;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetExpires(void)
{
    return urls->expires;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetLastModified(void)
{
    return urls->last_modified;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetServerDate(void)
{
    return urls->server_date;
}

NS_METHOD_(NPServerStatus)
nsPluginStreamPeer::GetServerStatus(void)
{
    return urls->server_status;
}
#endif
NS_METHOD
nsPluginStreamPeer::GetHeaderFieldCount(PRUint32 *result)
{
    *result = urls->all_headers.empty_index;
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetHeaderFieldKey(PRUint32 index, const char* *result)
{
    *result = urls->all_headers.key[index];
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::GetHeaderField(PRUint32 index, const char* *result)
{
    *result = urls->all_headers.value[index];
    return NS_OK;
}

NS_METHOD
nsPluginStreamPeer::RequestRead(nsByteRange* rangeList)
{
    NPError err = npn_requestread(stream->pstream,
                                  (NPByteRange*)rangeList);
    return fromNPError[err];
}

NS_IMPL_ADDREF(nsPluginStreamPeer);
NS_IMPL_RELEASE(nsPluginStreamPeer);

NS_METHOD
nsPluginStreamPeer::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if ((stream->seekable && aIID.Equals(kISeekablePluginStreamPeerIID)) ||
		aIID.Equals(kIPluginStreamPeer2IID) ||
        aIID.Equals(kIPluginStreamPeerIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*)(nsISupports*)(nsIPluginStreamPeer2*)this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
} 

#endif // !NEW_PLUGIN_STREAM_API

////////////////////////////////////////////////////////////////////////////////
