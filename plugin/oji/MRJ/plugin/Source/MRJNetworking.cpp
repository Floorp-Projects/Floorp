/*
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

extern nsIPluginManager* thePluginManager;		// now in badaptor.cpp.

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

class MRJInputStreamListener : public nsIPluginStreamListener {
public:
    NS_DECL_ISUPPORTS

    MRJInputStreamListener()
    {
        NS_INIT_ISUPPORTS();
    }
    
    ~MRJInputStreamListener() {}
    
    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo)
    {
        return NS_OK;
    }

    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length)
    {
        return NS_OK;
    }

    NS_IMETHOD
    OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName)
    {
        return NS_OK;
    }
 
    NS_IMETHOD
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status)
    {
        return NS_OK;
    }
    
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result)
    {
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(MRJInputStreamListener, nsIPluginStreamListener);

class MRJURLConnection {
public:
    MRJURLConnection(JMTextRef url, JMTextRef requestMethod,
                     JMURLConnectionOptions options,
                     JMAppletViewerRef appletViewer);

    ~MRJURLConnection();

    MRJPluginInstance* getInstance();

private:
    MRJPluginInstance* mInstance;
    char* mURL;
    char* mRequestMethod;
    JMURLConnectionOptions mOptions;
};

MRJURLConnection::MRJURLConnection(JMTextRef url, JMTextRef requestMethod,
                                   JMURLConnectionOptions options,
                                   JMAppletViewerRef appletViewer)
    :   mInstance(0), mURL(0), mRequestMethod(0), mOptions(options)
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
    if (connection->getInstance() == NULL) {
        delete connection;
        *urlConnectionRef = NULL;
        return paramErr;
    }
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
    return false;
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
	/* in URLConnectionRef = */ JMURLInputStreamRef iStreamRef,
	/* out numberOfProperties = */ int* numberOfProperties
	)
{
    if (numberOfProperties == NULL)
        return paramErr;
    *numberOfProperties = 0;
    return noErr;
}

static OSStatus getResponseProperties(
	/* in URLConnectionRef = */ JMURLInputStreamRef iStreamRef,
	/* in numberOfProperties = */ int numberOfProperties,
	/* out PropertyNames = */ JMTextRef* keys,
	/* out Values = */ JMTextRef* values
	)
{
    return paramErr;
}

static OSStatus openInputStream(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out URLStreamRef = */ JMURLInputStreamRef* urlInputStreamRef
	)
{
    return paramErr;
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
    return paramErr;
}

static OSStatus closeOutputStream(
	/* in URLOutputStreamRef = */ JMURLOutputStreamRef urlOutputStreamRef
	)
{
    return paramErr;
}

static OSStatus readInputStream(
	/* in URLConnectionRef = */ JMURLInputStreamRef iStreamRef,
	/* out Buffer = */ void* buffer,
	/* in BufferSize = */ UInt32 bufferSize,
	/* out BytesRead = */ SInt32* bytesRead
	)
{
    return paramErr;
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
    }
    return rv;
}

OSStatus CloseMRJNetworking(MRJSession* session)
{
    return noErr;
}
