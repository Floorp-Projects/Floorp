/*
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

/*
	JSEvaluator.h
 */

#pragma once

#include "nsIPluginStreamListener.h"

#ifndef JNI_H
#include <jni.h>
#endif

class MRJMonitor;
class MRJSession;
class MRJPluginInstance;

class JSEvaluator : public nsIPluginStreamListener {
public:
	NS_DECL_ISUPPORTS
	
	JSEvaluator(MRJPluginInstance* pluginInstance);
	virtual ~JSEvaluator();
	
    const char* eval(const char* script);
    
    const char* getResult()
    {
    	return mResult;
    }

	// nsIPluginStreamListener implementation.
	
    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @return The return value is currently ignored.  In the future it may be
     * used to cancel the URL load..
     */
    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo)
    {
    	return NS_OK;
    }

    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param aIStream  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the stream.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length);

    NS_IMETHOD
    OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName)
    {
		return NS_ERROR_NOT_IMPLEMENTED;
	}
	
    /**
     * Notify the observer that the URL has finished loading.  This method is 
     * called once when the networking library has finished processing the 
     * URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
     * 
     * This method is called regardless of whether the URL loaded successfully.<BR><BR>
     * 
     * @param status    Status code for the URL load.
     * @param msg   A text string describing the error.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status);

	/**
	 * What is this method supposed to do?
	 */
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result)
    {
    	*result = nsPluginStreamType_Normal;
    	return NS_OK;
    }

private:
	MRJPluginInstance*		mPluginInstance;
	MRJSession*				mSession;
	MRJMonitor*				mJSMonitor;
	char*					mScript;
	char*					mResult;
};
