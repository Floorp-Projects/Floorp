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
	nsLiveConnect.cpp
 */

#include "nsLiveconnect.h"
#include "MRJMonitor.h"
#include "MRJPlugin.h"
#include "nsIPluginManager.h"

#include <string.h>

extern nsIPluginManager* thePluginManager;

const InterfaceInfo nsLiveconnect::sInterfaces[] = {
	{ NS_ILIVECONNECT_IID, INTERFACE_OFFSET(nsLiveconnect, nsILiveconnect) },
	{ NS_IPLUGINSTREAMLISTENER_IID, INTERFACE_OFFSET(nsLiveconnect, nsIPluginStreamListener) },
};
const UInt32 nsLiveconnect::kInterfaceCount = sizeof(sInterfaces) / sizeof(InterfaceInfo);

nsLiveconnect::nsLiveconnect()
	:	SupportsMixin(this, sInterfaces, kInterfaceCount),
		mJavaScriptMonitor(NULL), mScript(NULL), mResult(NULL)
{
}

nsLiveconnect::~nsLiveconnect()
{
	if (mJavaScriptMonitor != NULL)
		delete mJavaScriptMonitor;
}

static char* u2c(const jchar *ustr, jsize length)
{
	char* result = new char[length + 1];
	if (result != NULL) {
		char* cstr = result;
		while (length-- > 0) {
			*cstr++ = (char) *ustr++;
		}
		*cstr = '\0';
	}
	return result;
}

const char* kJavaScriptPrefix = "javascript:";

NS_METHOD	
nsLiveconnect::Eval(JNIEnv *env, jsobject obj, const jchar *script, jsize length, void* principalsArray[], 
 		        int numPrincipals, void *securityContext, jobject *outResult)
{
	MRJPluginInstance* pluginInstance = (MRJPluginInstance*) obj;
	nsIPluginStreamListener* listener = this;

	if (mJavaScriptMonitor == NULL)
		mJavaScriptMonitor = new MRJMonitor(pluginInstance->getSession());

	mJavaScriptMonitor->enter();

	while (mScript != NULL) {
		// some other thread is evaluating a script.
		mJavaScriptMonitor->wait();
	}
	
	// convert the script to ASCII, construct a "javascript:" URL.
	char* cscript = u2c(script, length);
	mScript = new char[strlen(kJavaScriptPrefix) + length + 1];
	strcpy(mScript, kJavaScriptPrefix);
	strcat(mScript, cscript);
	delete[] cscript;
	nsresult result = thePluginManager->GetURL((nsIPluginInstance*)pluginInstance, mScript, NULL, listener);
	
	// need to block until the result is ready.
	mJavaScriptMonitor->wait();
	
	// default result is NULL, in case JavaScript returns undefined value.
	*outResult = NULL;

	// result should now be ready, convert it to a Java string and return.
	if (mResult != NULL) {
		*outResult = env->NewStringUTF(mResult);
		delete[] mResult;
		mResult = NULL;
	}
	
	delete[] mScript;
	mScript = NULL;
	
	mJavaScriptMonitor->notifyAll();
	
	mJavaScriptMonitor->exit();
	
	return NS_OK;
}

NS_METHOD nsLiveconnect::OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length)
{
	// hopefully all our data is available.
	mResult = new char[length + 1];
	if (mResult != NULL) {
		if (input->Read(mResult, length, &length) == NS_OK) {
			// We've delayed processing the applet tag, because we
			// don't know the location of the curren document yet.
			mResult[length] = '\0';
		}
	}
	return NS_OK;
}

NS_METHOD nsLiveconnect::OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status)
{
	// the stream has been closed, notify any waiting Java threads.
	mJavaScriptMonitor->notifyAll();
	return NS_OK;
}
