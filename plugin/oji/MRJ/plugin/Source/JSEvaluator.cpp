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
	JSEvaluator.cpp
 */

#include "JSEvaluator.h"
#include "MRJPlugin.h"
#include "MRJSession.h"
#include "MRJMonitor.h"
#include "nsIPluginManager.h"

#include <string.h>

extern nsIPluginManager* thePluginManager;

JSEvaluator::JSEvaluator(MRJPluginInstance* pluginInstance)
	:	mPluginInstance(pluginInstance)
{
	NS_ADDREF(pluginInstance);
	mSession = pluginInstance->getSession();
	mJSMonitor = new MRJMonitor(mSession);
}

JSEvaluator::~JSEvaluator()
{
	NS_IF_RELEASE(mPluginInstance);
	if (mJSMonitor != NULL)
		delete mJSMonitor;
}

NS_IMPL_ISUPPORTS(JSEvaluator, nsIPluginStreamListener::GetIID())

const char* JSEvaluator::eval(const char* script)
{
	JNIEnv* env = mSession->getCurrentEnv();
	nsIPluginStreamListener* listener = this;

	mJSMonitor->enter();

	while (mScript != NULL) {
		// some other thread is evaluating a script.
		mJSMonitor->wait();
	}
	
	// construct a "javascript:" URL from the passed-in script.
	const char* kJavaScriptPrefix = "javascript:";
	mScript = new char[strlen(kJavaScriptPrefix) + strlen(script) + 1];
	if (mScript != NULL) {
		strcpy(mScript, kJavaScriptPrefix);
		strcat(mScript, script);

		// start an async evaluation of this script.
		nsresult result = thePluginManager->GetURL((nsIPluginInstance*)mPluginInstance, mScript, NULL, (nsIPluginStreamListener*)this);
		
		// default result is NULL, in case JavaScript returns undefined value.
		if (mResult != NULL) {
			delete[] mResult;
			mResult = NULL;
		}

		// need to block until the result is ready.
		mJSMonitor->wait();
		
		// can now delete the script.
		delete[] mScript;
		mScript = NULL;
	}
	
	mJSMonitor->notifyAll();
	
	mJSMonitor->exit();
	
	return mResult;
}

NS_METHOD JSEvaluator::OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length)
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

NS_METHOD JSEvaluator::OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status)
{
	// the stream has been closed, notify any waiting Java threads.
	mJSMonitor->notifyAll();
	return NS_OK;
}
