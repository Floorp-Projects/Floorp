/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */

#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"
#include "nsSoftwareUpdateIIDs.h"

#include "nsInstall.h"
//#include "zipfile.h" // replaced by nsIJAR.h

#include "nsRepository.h"
#include "nsIServiceManager.h"

#include "nsSpecialSystemDirectory.h"
#include "nsFileStream.h"

#include "nspr.h"
#include "jsapi.h"

#include "nsIEventQueueService.h"
#include "nsIEnumerator.h"
#include "nsIJAR.h"

static NS_DEFINE_IID(kISoftwareUpdateIID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);




static JSClass global_class = 
{
    "global", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};


extern PRInt32 InitXPInstallObjects(JSContext *jscontext, JSObject *global, const char* jarfile, const PRUnichar* url, const PRUnichar* args);
extern nsresult InitInstallVersionClass(JSContext *jscontext, JSObject *global, void** prototype);
extern nsresult InitInstallTriggerGlobalClass(JSContext *jscontext, JSObject *global, void** prototype);

// Defined in this file:
static void     XPInstallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);
static nsresult GetInstallScriptFromJarfile(const char* jarFile, char** scriptBuffer, PRUint32 *scriptLength);
static nsresult SetupInstallContext(const char* jarFile, const PRUnichar* url, const PRUnichar* args, JSRuntime **jsRT, JSContext **jsCX, JSObject **jsGlob);

extern "C" void RunInstallOnThread(void *data);

///////////////////////////////////////////////////////////////////////////////////////////////
// Function name	: XPInstallErrorReporter
// Description	    : Prints error message to stdout
// Return type		: void
// Argument         : JSContext *cx
// Argument         : const char *message
// Argument         : JSErrorReport *report
///////////////////////////////////////////////////////////////////////////////////////////////
static void
XPInstallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    int i, j, k, n;

    fputs("xpinstall: ", stderr);
    if (!report) 
    {
		fprintf(stderr, "%s\n", message);
		return;
    }

    if (report->filename)
		fprintf(stderr, "%s, ", report->filename);
    if (report->lineno)
		fprintf(stderr, "line %u: ", report->lineno);
    fputs(message, stderr);
    if (!report->linebuf) 
    {
		putc('\n', stderr);
		return;
    }

    fprintf(stderr, ":\n%s\n", report->linebuf);
    n = report->tokenptr - report->linebuf;
    for (i = j = 0; i < n; i++) {
		if (report->linebuf[i] == '\t') {
			for (k = (j + 8) & ~7; j < k; j++)
				putc('.', stderr);
			continue;
		}
		putc('.', stderr);
		j++;
    }
    fputs("^\n", stderr);
}





///////////////////////////////////////////////////////////////////////////////////////////////
// Function name	: GetInstallScriptFromJarfile
// Description	    : Extracts and reads in a install.js file from a passed jar file.
// Return type		: static nsresult 
// Argument         : const char* jarFile     - **NSPR** filepath
// Argument         : char** scriptBuffer     - must be deleted via delete []
// Argument         : PRUint32 *scriptLength
///////////////////////////////////////////////////////////////////////////////////////////////

static nsresult 
GetInstallScriptFromJarfile(const char* jarFile, char** scriptBuffer, PRUint32 *scriptLength)
{
    nsIJAR* hZip = nsnull;
    PRInt32 result;
    
    *scriptBuffer = nsnull;
    *scriptLength = 0;

    static NS_DEFINE_IID(kIJARIID, NS_IJAR_IID);
    static NS_DEFINE_IID(kJARCID,  NS_JAR_CID);
    nsresult rv = nsComponentManager::CreateInstance(kJARCID, nsnull, kIJARIID, 
                                                     (void**) &hZip);
    // Open the jarfile
    if (hZip)
        rv = hZip->Open( jarFile, &result );
    if (NS_FAILED(rv))
    {
        NS_IF_RELEASE(hZip);
        return rv;
    }
    if (result != 0)
    {
        NS_IF_RELEASE(hZip);
        return NS_ERROR_FAILURE;
    }


    // Read manifest file for Install Script filename.
    //FIX:  need to do.
    
    // Extract the install.js file to the temporary directory
    nsSpecialSystemDirectory installJSFileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    installJSFileSpec += "install.js";
    installJSFileSpec.MakeUnique();

    // Extract the install.js file.
    rv = hZip->Extract( "install.js", nsNSPRPath(installJSFileSpec), &result );
    if (NS_FAILED(rv))
    {
        NS_IF_RELEASE( hZip );
        return rv;
    }
    if (result != 0)
    {
        NS_IF_RELEASE(hZip);
        return NS_ERROR_FAILURE;
    }

    // Read it into a buffer
    char* buffer;
    PRUint32 bufferLength;
    PRUint32 readLength;

    nsInputFileStream fileStream(installJSFileSpec);
    (fileStream.GetIStream())->GetLength(&bufferLength);
    buffer = new char[bufferLength + 1];
    
    if (buffer == nsnull)
        return NS_ERROR_FAILURE;

    rv = (fileStream.GetIStream())->Read(buffer, bufferLength, &readLength);

    if (NS_SUCCEEDED(rv))
    {
        *scriptBuffer = buffer;
        *scriptLength = readLength;
    }
    else
    {
        delete [] buffer;
    }

    NS_IF_RELEASE( hZip );
    fileStream.close();
    installJSFileSpec.Delete(PR_FALSE);
        
    return rv;   
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Function name	: SetupInstallContext
// Description	    : Creates a Javascript runtime and adds our xpinstall objects to it.
// Return type		: static nsresult
// Argument         : const char* jarFile - native filepath to where jar exists on disk 
// Argument         : const PRUnichar* url  - URL of where this package came from
// Argument         : const PRUnichar* args    - any arguments passed into the javascript context
// Argument         : JSRuntime **jsRT   - Must be deleted via JS_DestroyRuntime
// Argument         : JSContext **jsCX   - Must be deleted via JS_DestroyContext
// Argument         : JSObject **jsGlob
///////////////////////////////////////////////////////////////////////////////////////////////
static nsresult SetupInstallContext(const char* jarFile,
                                    const PRUnichar* url,
                                    const PRUnichar* args, 
                                    JSRuntime **jsRT, 
                                    JSContext **jsCX, 
                                    JSObject **jsGlob)
{
    JSRuntime   *rt;
	JSContext   *cx;
    JSObject    *glob;
    
    *jsRT   = nsnull;
    *jsCX   = nsnull;
    *jsGlob = nsnull;

    // JS init
	rt = JS_Init(8L * 1024L * 1024L);
	if (!rt)
        return NS_ERROR_OUT_OF_MEMORY;
    else
    {
        cx = JS_NewContext(rt, 8192);
	    if (!cx)
        {
            JS_DestroyRuntime(rt);
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    JS_SetErrorReporter(cx, XPInstallErrorReporter);

    // new global object
    glob = JS_NewObject(cx, &global_class, nsnull, nsnull);
	
    // Init standard classes
    JS_InitStandardClasses(cx, glob);

    // Add our Install class to this context
    InitXPInstallObjects(cx, glob, jarFile, url, args);
    InitInstallVersionClass(cx, glob, nsnull);
    InitInstallTriggerGlobalClass(cx, glob, nsnull);

    *jsRT   = rt;
    *jsCX   = cx;
    *jsGlob = glob;

    return NS_OK;
}




///////////////////////////////////////////////////////////////////////////////////////////////
// Function name	: RunInstall
// Description	    : Creates our Install Thread.
// Return type		: PRInt32 
// Argument         : nsInstallInfo *installInfo
///////////////////////////////////////////////////////////////////////////////////////////////
PRInt32 RunInstall(nsInstallInfo *installInfo)
{   
    if (installInfo->GetFlags() == 0x0000FFFF) // XXX bogus value -- do we want this feature?
    {
        RunInstallOnThread((void *)installInfo);
    }
    else
    {
        PR_CreateThread(PR_USER_THREAD,
                        RunInstallOnThread,
                        (void*)installInfo, 
                        PR_PRIORITY_NORMAL, 
                        PR_GLOBAL_THREAD, 
                        PR_UNJOINABLE_THREAD,
                        0);  
    }
    return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Function name	: RunInstallOnThread
// Description	    : called by starting thread.  It directly calls the C api for xpinstall, 
//                  : and once that returns, it calls the completion routine to notify installation
//                  : completion.
// Return type		: extern "C" 
// Argument         : void *data
///////////////////////////////////////////////////////////////////////////////////////////////
extern "C" void RunInstallOnThread(void *data)
{
    nsInstallInfo *installInfo = (nsInstallInfo*)data;
    
    char        *scriptBuffer;
    PRUint32    scriptLength;

    JSRuntime   *rt;
	JSContext   *cx;
    JSObject    *glob;

    nsIXPINotifier *notifier;
    nsresult    rv;

    NS_WITH_SERVICE(nsISoftwareUpdate, softwareUpdate, kSoftwareUpdateCID, &rv );

    if (!NS_SUCCEEDED(rv))
    {
        NS_WARNING("shouldn't have RunInstall() if we can't get SoftwareUpdate");
        return;
    }

    softwareUpdate->SetActiveNotifier( installInfo->GetNotifier() );
    softwareUpdate->GetMasterNotifier(&notifier);
    
    nsString url;
    installInfo->GetURL(url);

    if(notifier)
        notifier->BeforeJavascriptEvaluation( url.GetUnicode() );
    
    nsString args;
    installInfo->GetArguments(args);

    char *jarpath;
    installInfo->GetLocalFile(&jarpath);

    if (!jarpath)
        goto bail;

    rv = GetInstallScriptFromJarfile( jarpath, 
                                      &scriptBuffer, 
                                      &scriptLength);

    // XXX memory leaks!
    if (NS_FAILED(rv) || scriptBuffer == nsnull)
        goto bail;

    rv = SetupInstallContext(   jarpath, 
                                url.GetUnicode(),
                                args.GetUnicode(), 
                                &rt, &cx, &glob);
    if (NS_FAILED(rv))
        goto bail;
    
    // Go ahead and run!!
    jsval rval;

    JS_EvaluateScript(cx, 
                      glob,
                      scriptBuffer, 
                      scriptLength,
                      nsnull,
                      0,
                      &rval);

    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);

bail:
    if(notifier) 
        notifier->AfterJavascriptEvaluation( url.GetUnicode() );

    if (scriptBuffer) delete [] scriptBuffer;
    if (jarpath) {
        PR_Delete(jarpath);
        nsCRT::free(jarpath);
    }

    softwareUpdate->SetActiveNotifier(0);
    softwareUpdate->InstallJarCallBack();
}
