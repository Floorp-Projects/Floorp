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
	nsLiveConnect.h
 */

#pragma once

#include "nsILiveconnect.h"
#include "nsIPluginStreamListener.h"
#include "SupportsMixin.h"

class MRJMonitor;

class nsLiveconnect : public nsILiveconnect,
					  public nsIPluginStreamListener,
					  public SupportsMixin {
public:
	DECL_SUPPORTS_MIXIN
	
	nsLiveconnect();
	virtual ~nsLiveconnect();
	
    /**
     * get member of a Native JSObject for a given name.
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a member.
     * @param pjobj      - return parameter as a java object representing 
     *                     the member. If it is a basic data type it is converted to
     *                     a corresponding java type. If it is a NJSObject, then it is
     *                     wrapped up as java wrapper netscape.javascript.JSObject.
     */
    NS_IMETHOD
    GetMember(JNIEnv *env, jsobject jsobj, const jchar *name, jsize length, void* principalsArray[], 
                     int numPrincipals, void *securityContext, jobject *pjobj)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * get member of a Native JSObject for a given index.
     *
     * @param obj        - A Native JS Object.
     * @param slot      - Index of a member.
     * @param pjobj      - return parameter as a java object representing 
     *                     the member. 
     */
    NS_IMETHOD
    GetSlot(JNIEnv *env, jsobject jsobj, jint slot, void* principalsArray[], 
                     int numPrincipals, void *securityContext, jobject *pjobj)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * set member of a Native JSObject for a given name.
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a member.
     * @param jobj       - Value to set. If this is a basic data type, it is converted
     *                     using standard JNI calls but if it is a wrapper to a JSObject
     *                     then a internal mapping is consulted to convert to a NJSObject.
     */
    NS_IMETHOD
    SetMember(JNIEnv *env, jsobject jsobj, const jchar* name, jsize length, jobject jobj, void* principalsArray[], 
                     int numPrincipals, void *securityContext)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * set member of a Native JSObject for a given index.
     *
     * @param obj        - A Native JS Object.
     * @param index      - Index of a member.
     * @param jobj       - Value to set. If this is a basic data type, it is converted
     *                     using standard JNI calls but if it is a wrapper to a JSObject
     *                     then a internal mapping is consulted to convert to a NJSObject.
     */
    NS_IMETHOD
    SetSlot(JNIEnv *env, jsobject jsobj, jint slot, jobject jobj, void* principalsArray[], 
                     int numPrincipals, void *securityContext)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * remove member of a Native JSObject for a given name.
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a member.
     */
    NS_IMETHOD
    RemoveMember(JNIEnv *env, jsobject jsobj, const jchar* name, jsize length,  void* principalsArray[], 
                     int numPrincipals, void *securityContext)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * call a method of Native JSObject. 
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a method.
     * @param jobjArr    - Array of jobjects representing parameters of method being caled.
     * @param pjobj      - return value.
     */
    NS_IMETHOD
    Call(JNIEnv *env, jsobject jsobj, const jchar* name, jsize length, jobjectArray jobjArr,  void* principalsArray[], 
                     int numPrincipals, void *securityContext, jobject *pjobj)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * Evaluate a script with a Native JS Object representing scope.
     *
     * @param obj                - A Native JS Object.
     * @param principalsArray    - Array of principals to be used to compare privileges.
     * @param numPrincipals      - Number of principals being passed.
     * @param script             - Script to be executed.
     * @param pjobj              - return value.
     */
    NS_IMETHOD	
    Eval(JNIEnv *env, jsobject obj, const jchar *script, jsize length, void* principalsArray[], 
         int numPrincipals, void *securityContext, jobject *outResult);

    /**
     * Get the window object for a plugin instance.
     *
     * @param pJavaObject        - Either a jobject or a pointer to a plugin instance 
     *                             representing the java object.
     * @param pjobj              - return value. This is a native js object 
     *                             representing the window object of a frame 
     *                             in which a applet/bean resides.
     */
    NS_IMETHOD
    GetWindow(JNIEnv *env, void *pJavaObject, void* principalsArray[], 
                     int numPrincipals, void *securityContext, jsobject *pobj)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * Finalize a JSObject instance.
     *
     * @param env       - JNIEnv on which the call is being made.
     * @param obj        - A Native JS Object.
     */
    NS_IMETHOD
    FinalizeJSObject(JNIEnv *env, jsobject jsobj)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * Convert a JSObject to a string.
     *
     * @param env       - JNIEnv on which the call is being made.
     * @param obj        - A Native JS Object.
     * @param jstring    - Return value as a jstring representing a JS object.
     */
    NS_IMETHOD
    ToString(JNIEnv *env, jsobject obj, jstring *pjstring)
	{
		return NS_ERROR_NOT_IMPLEMENTED;
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
	MRJMonitor* mJavaScriptMonitor;
	char* mScript;
	char* mResult;

	// support for SupportsMixin.
	static const InterfaceInfo sInterfaces[];
	static const UInt32 kInterfaceCount;
};
