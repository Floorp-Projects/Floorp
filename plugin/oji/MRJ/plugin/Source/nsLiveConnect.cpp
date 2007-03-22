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
    :   SupportsMixin(this, sInterfaces, kInterfaceCount),
        mJavaScriptMonitor(NULL), mScript(NULL), mResult(NULL)
{
}

nsLiveconnect::~nsLiveconnect()
{
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
                    int numPrincipals, nsISupports *securitySupports, jobject *outResult)
{
    MRJPluginInstance* pluginInstance = (MRJPluginInstance*) obj;
    nsIPluginStreamListener* listener = this;

    if (!mJavaScriptMonitor) {
        mJavaScriptMonitor = new MRJMonitor(pluginInstance->getSession());
        if (!mJavaScriptMonitor)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    mJavaScriptMonitor->enter();

    while (mScript != NULL) {
        // some other thread is evaluating a script.
        mJavaScriptMonitor->wait();
    }

    nsresult rv = NS_OK;    
    // convert the script to ASCII, construct a "javascript:" URL.
    char* cscript = u2c(script, length);
    if (!cscript) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        mScript = new char[strlen(kJavaScriptPrefix) + length + 1];
        if (!mScript) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
            strcpy(mScript, kJavaScriptPrefix);
            strcat(mScript, cscript);
            delete[] cscript;
            rv = thePluginManager->GetURL((nsIPluginInstance*)pluginInstance, mScript, NULL, listener);

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
        }
    }

    mJavaScriptMonitor->notifyAll();

    mJavaScriptMonitor->exit();

    return rv;
}

NS_METHOD nsLiveconnect::OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length)
{
    // hopefully all our data is available.
    mResult = new char[length + 1];
    if (mResult != NULL) {
        if (input->Read(mResult, length, &length) == NS_OK) {
            // We've delayed processing the applet tag, because we
            // don't know the location of the current document yet.
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
