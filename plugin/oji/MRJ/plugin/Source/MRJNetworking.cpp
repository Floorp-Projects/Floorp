/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
    MRJNetworking.cpp
    
    by Patrick C. Beard.
 */

#include <TextCommon.h>
#include <JManager.h>
#include "JMURLConnection.h"

#include "MRJNetworking.h"
#include "MRJContext.h"
#include "MRJPlugin.h"
#include "MRJSession.h"
#include "MRJMonitor.h"
#include "nsIPluginManager2.h"

#include <vector>

extern nsIPluginManager* thePluginManager;		// now in badaptor.cpp.
extern nsIPluginManager2* thePluginManager2;

static char* JMTextToEncoding(JMTextRef textRef, JMTextEncoding encoding)
{
	UInt32 length = 0;
    OSStatus status = ::JMGetTextLengthInBytes(textRef, encoding, &length);
    if (status != noErr)
        return NULL;
    char* text = new char[length + 1];
    if (text != NULL) {
        UInt32 actualLength;
    	status = ::JMGetTextBytes(textRef, encoding, text, length, &actualLength);
	    if (status != noErr) {
	        delete text;
	        return NULL;
	    }
	    text[length] = '\0';
    }
    return text;
}

class MRJInputStream : public nsIPluginStreamListener {
    MRJMonitor mMonitor;
    typedef std::vector<char> buffer_t;
    buffer_t mBuffer;
    size_t mOffset;
    bool mComplete;
public:
    NS_DECL_ISUPPORTS

    MRJInputStream(MRJSession* session)
        :   mMonitor(session), mOffset(0), mComplete(false)
    {
        mBuffer.reserve(8192);
    }
    
    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo)
    {
        return NS_OK;
    }

    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length);

    NS_IMETHOD
    OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName)
    {
        return NS_OK;
    }
 
    NS_IMETHOD
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status)
    {
        if (!mComplete) {
            mComplete = true;
            mMonitor.notify();
        }
        return NS_OK;
    }
    
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result)
    {
        *result = nsPluginStreamType_Normal;
        return NS_OK;
    }

    OSStatus read(void* buffer, SInt32 bufferSize, SInt32* bytesRead);
};
NS_IMPL_ISUPPORTS1(MRJInputStream, nsIPluginStreamListener)

NS_IMETHODIMP
MRJInputStream::OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length)
{
    size_t oldSize = mBuffer.size();
    mBuffer.resize(oldSize + length);
    buffer_t::iterator buffer = mBuffer.begin() + oldSize;
    input->Read(buffer, length, &length);
    mMonitor.notify();
    return NS_OK;
}

OSStatus MRJInputStream::read(void* buffer, SInt32 bufferSize, SInt32* bytesRead)
{
    size_t sz = mBuffer.size();
    while (mOffset >= sz && !mComplete) {
        // wait until there is some data to read.
        mMonitor.wait();
        sz = mBuffer.size();
    }
    
    SInt32 available = (sz - mOffset);
    if (bufferSize > available)
        bufferSize = available;

    if (bufferSize <= 0 && mComplete) {
        *bytesRead = 0;
        return noErr;
    }

    ::BlockMoveData(mBuffer.begin() + mOffset, buffer, bufferSize);
    *bytesRead = bufferSize;
    mOffset += bufferSize;
    
    return noErr;
}

class MRJURLConnection {
public:
    MRJURLConnection(JMTextRef url, JMTextRef requestMethod,
                     JMURLConnectionOptions options,
                     JMAppletViewerRef appletViewer);

    ~MRJURLConnection();

    MRJPluginInstance* getInstance();
    const char* getURL();
    const char* getRequestMethod();
    Boolean getUsingProxy();

private:
    MRJPluginInstance* mInstance;
    char* mURL;
    char* mRequestMethod;
    JMURLConnectionOptions mOptions;
    Boolean mUsingProxy;
};

MRJURLConnection::MRJURLConnection(JMTextRef url, JMTextRef requestMethod,
                                   JMURLConnectionOptions options,
                                   JMAppletViewerRef appletViewer)
    :   mInstance(0), mURL(0), mRequestMethod(0), mOptions(options), mUsingProxy(false)
{
    MRJPluginInstance* instance = MRJPluginInstance::getInstances();
    if (appletViewer != NULL) {
        while (instance != NULL) {
            MRJContext* context = instance->getContext();
            if (context->getViewerRef() == appletViewer) {
                mInstance = instance;
                break;
            }
            instance = instance->getNextInstance();
        }
    } else {
        // any instance will do?
        mInstance = instance;
        NS_IF_ADDREF(instance);
    }
    
    TextEncoding utf8 = ::CreateTextEncoding(kTextEncodingUnicodeDefault,
                                             kTextEncodingDefaultVariant,
                                             kUnicodeUTF8Format);
    
    // pull the text out of the url and requestMethod.
    mURL = ::JMTextToEncoding(url, utf8);
    mRequestMethod = ::JMTextToEncoding(requestMethod, utf8);
    
    // see if a proxy will be used for this URL.
    if (thePluginManager2 != NULL) {
        char* proxyForURL = NULL;
        if (thePluginManager2->FindProxyForURL(mURL, &proxyForURL) == NS_OK) {
            mUsingProxy = (::strcmp("DIRECT", proxyForURL) != 0);
            delete[] proxyForURL;
        }
    }
}

MRJURLConnection::~MRJURLConnection()
{
    delete[] mURL;
    delete[] mRequestMethod;
}

inline MRJPluginInstance* MRJURLConnection::getInstance()
{
    return mInstance;
}

inline const char* MRJURLConnection::getURL()
{
    return mURL;
}

inline const char* MRJURLConnection::getRequestMethod()
{
    return mRequestMethod;
}

inline Boolean MRJURLConnection::getUsingProxy()
{
    return mUsingProxy;
}

static OSStatus openConnection(
	/* in URL = */ JMTextRef url,
	/* in RequestMethod = */ JMTextRef requestMethod,
	/* in ConnectionOptions = */ JMURLConnectionOptions options,
	/* in AppletViewer = */ JMAppletViewerRef appletViewer,
	/* out URLConnectionRef = */ JMURLConnectionRef* urlConnectionRef
	)
{
    MRJURLConnection* connection = new MRJURLConnection(url, requestMethod,
                                                        options, appletViewer);
    *urlConnectionRef = connection;
    return noErr;
}

static OSStatus closeConnection(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef
	)
{
    MRJURLConnection* connection = reinterpret_cast<MRJURLConnection*>(urlConnectionRef);
    delete connection;
    return noErr;
}

static Boolean usingProxy(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef
	)
{
    MRJURLConnection* connection = reinterpret_cast<MRJURLConnection*>(urlConnectionRef);
    return connection->getUsingProxy();
}

static OSStatus getCookie(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out CookieValue = */ JMTextRef* cookie
	)
{
    return paramErr;
}

static OSStatus setCookie(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* in CookieValue = */ JMTextRef cookie
	)
{
    return paramErr;
}

static OSStatus setRequestProperties(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* in NumberOfProperties = */ int numberOfProperties,
	/* in PropertyNames = */ JMTextRef* keys,
	/* in Values = */ JMTextRef* value
	)
{
    return paramErr;
}

static OSStatus getResponsePropertiesCount(
	/* in URLConnectionRef = */ JMURLInputStreamRef urlInputStreamRef,
	/* out numberOfProperties = */ int* numberOfProperties
	)
{
    if (numberOfProperties == NULL)
        return paramErr;
    *numberOfProperties = 0;
    return noErr;
}

static OSStatus getResponseProperties(
	/* in URLConnectionRef = */ JMURLInputStreamRef urlInputStreamRef,
	/* in numberOfProperties = */ int numberOfProperties,
	/* out PropertyNames = */ JMTextRef* keys,
	/* out Values = */ JMTextRef* values
	)
{
    return noErr;
}

static OSStatus openInputStream(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out URLStreamRef = */ JMURLInputStreamRef* urlInputStreamRef
	)
{
    MRJURLConnection* connection = reinterpret_cast<MRJURLConnection*>(urlConnectionRef);
    MRJInputStream* inputStream = new MRJInputStream(connection->getInstance()->getSession());
    if (!inputStream)
        return memFullErr;

    inputStream->AddRef();
    *urlInputStreamRef = inputStream;

    nsIPluginInstance* pluginInstance = connection->getInstance();
    nsIPluginStreamListener* listener = inputStream;
    nsresult rv = thePluginManager->GetURL(pluginInstance, connection->getURL(), NULL, listener);

    return noErr;
}

static OSStatus openOutputStream(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out URLOutputStreamRef = */ JMURLOutputStreamRef* urlOutputStreamRef
	)
{
    return paramErr;
}

static OSStatus closeInputStream(
	/* in URLInputStreamRef = */ JMURLInputStreamRef urlInputStreamRef
	)
{
    MRJInputStream* inputStream = reinterpret_cast<MRJInputStream*>(urlInputStreamRef);
    inputStream->Release();
    return noErr;
}

static OSStatus closeOutputStream(
	/* in URLOutputStreamRef = */ JMURLOutputStreamRef urlOutputStreamRef
	)
{
    return paramErr;
}

static OSStatus readInputStream(
	/* in URLConnectionRef = */ JMURLInputStreamRef urlInputStreamRef,
	/* out Buffer = */ void* buffer,
	/* in BufferSize = */ UInt32 bufferSize,
	/* out BytesRead = */ SInt32* bytesRead
	)
{
    MRJInputStream* inputStream = reinterpret_cast<MRJInputStream*>(urlInputStreamRef);
    return inputStream->read(buffer, bufferSize, bytesRead);
}

static OSStatus writeOutputStream(
	/* in URLConnectionRef = */ JMURLOutputStreamRef oStreamRef,
	/* in Buffer = */ void* buffer,
	/* in BytesToWrite = */ SInt32 bytesToWrite
	)
{
    return paramErr;
}

static JMURLCallbacks theURLCallbacks = {
    kJMVersion,
	&openConnection, &closeConnection,
	&usingProxy, &getCookie, &setCookie,
	&setRequestProperties,
	&getResponsePropertiesCount,
	&getResponseProperties,
    &openInputStream, &openOutputStream,
    &closeInputStream, &closeOutputStream,
    &readInputStream, &writeOutputStream
};

OSStatus OpenMRJNetworking(MRJSession* session)
{
    OSStatus rv = paramErr;
    if (&::JMURLSetCallbacks != 0) {
        rv = ::JMURLSetCallbacks(session->getSessionRef(),
                                 "http", &theURLCallbacks);
        rv = ::JMURLSetCallbacks(session->getSessionRef(),
                                 "https", &theURLCallbacks);
    }
    return rv;
}

OSStatus CloseMRJNetworking(MRJSession* session)
{
    return noErr;
}
