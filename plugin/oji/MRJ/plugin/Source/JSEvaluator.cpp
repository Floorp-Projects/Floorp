/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

NS_IMPL_ISUPPORTS1(JSEvaluator, nsIPluginStreamListener)

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
