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
#include "zipfile.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

#include "nsSpecialSystemDirectory.h"
#include "nsFileStream.h"

#include "nspr.h"
#include "jsapi.h"

#if 0
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"

#include "nsIURL.h"

#include "nsAppShellProxy.h"


static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_IID(kIAppShellServiceIID,   NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);
#endif

static NS_DEFINE_IID(kISoftwareUpdateIID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);




static JSClass global_class = 
{
    "global", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};


extern PRInt32 InitXPInstallObjects(JSContext *jscontext, JSObject *global, const char* jarfile, const char* args);

// Defined in this file:
static void     XPInstallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);
static nsresult GetInstallScriptFromJarfile(const char* jarFile, char** scriptBuffer, PRUint32 *scriptLength);
static nsresult SetupInstallContext(const char* jarFile, const char* args, JSRuntime **jsRT, JSContext **jsCX, JSObject **jsGlob);

extern "C" void RunInstallOnThread(void *data);

extern "C" NS_EXPORT PRInt32 RunInstall(const char* jarFile, const char* flags, const char* args, const char* fromURL);



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
// Argument         : const char* jarFile     - native filepath
// Argument         : char** scriptBuffer     - must be deleted via delete []
// Argument         : PRUint32 *scriptLength
///////////////////////////////////////////////////////////////////////////////////////////////

static nsresult 
GetInstallScriptFromJarfile(const char* jarFile, char** scriptBuffer, PRUint32 *scriptLength)
{
    // Open the jarfile.
    void* hZip;
    
    *scriptBuffer = nsnull;
    *scriptLength = 0;

    nsresult rv = ZIP_OpenArchive(jarFile ,  &hZip);
    
    if (rv != ZIP_OK)
        return rv;


    // Read manifest file for Install Script filename.
    //FIX:  need to do.
    
    // Extract the install.js file to the temporary directory
    nsSpecialSystemDirectory installJSFileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    installJSFileSpec += "install.js";
    installJSFileSpec.MakeUnique();

    // Extract the install.js file.
    rv  = ZIP_ExtractFile( hZip, "install.js", nsprPath(installJSFileSpec) );
    if (rv != ZIP_OK)
    {
        ZIP_CloseArchive(&hZip);
        return rv;
    }
    
    // Read it into a buffer
    char* buffer;
    PRUint32 bufferLength;
    PRUint32 readLength;

    nsInputFileStream fileStream(installJSFileSpec);
    (fileStream.GetIStream())->GetLength(&bufferLength);
    buffer = new char[bufferLength + 1];

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

    ZIP_CloseArchive(&hZip);
    fileStream.close();
    installJSFileSpec.Delete(PR_FALSE);
        
    return rv;   
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Function name	: SetupInstallContext
// Description	    : Creates a Javascript runtime and adds our xpinstall objects to it.
// Return type		: static nsresult
// Argument         : const char* jarFile - native filepath to where jar exists on disk 
// Argument         : const char* args    - any arguments passed into the javascript context
// Argument         : JSRuntime **jsRT   - Must be deleted via JS_DestroyRuntime
// Argument         : JSContext **jsCX   - Must be deleted via JS_DestroyContext
// Argument         : JSObject **jsGlob
///////////////////////////////////////////////////////////////////////////////////////////////
static nsresult SetupInstallContext(const char* jarFile, 
                                    const char* args, 
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
    {
        return -1;
    }

    // new context
    cx = JS_NewContext(rt, 8192);
	if (!rt)
    {
        return -1;
    }

    JS_SetErrorReporter(cx, XPInstallErrorReporter);

    // new global object
    glob = JS_NewObject(cx, &global_class, nsnull, nsnull);
	
    // Init standard classes
    JS_InitStandardClasses(cx, glob);

    // Add our Install class to this context
    InitXPInstallObjects(cx, glob, jarFile, args);

    // Fix:  We have to add Version and Trigger to this context!!
    
    
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
#if 0
    // We are one the UI Thread.  Get and save the eventQueue.
    // Create the Event Queue for the UI thread...
    nsIEventQueueService* eventQService;
    nsresult rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                               kIEventQueueServiceIID,
                                               (nsISupports **)&eventQService);
    PLEventQueue* thisEventQueue;
    if (NS_OK == rv) 
    {

        rv = eventQService->GetThreadEventQueue(PR_GetCurrentThread(), 
                                                &thisEventQueue);

    }

    installInfo->SetUIEventQueue(thisEventQueue);
#endif
    PR_CreateThread(PR_USER_THREAD,
                    RunInstallOnThread,
                    (void*)installInfo, 
                    PR_PRIORITY_NORMAL, 
                    PR_GLOBAL_THREAD, 
                    PR_UNJOINABLE_THREAD,
                    0);  
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
    nsresult rv;


#if 0  
  /*
   * Create the Application Shell instance...
   */
  nsIAppShellService* appShell;
  rv = nsServiceManager::GetService(kAppShellServiceCID,
                                    kIAppShellServiceIID,
                                    (nsISupports**)&appShell);
  if (NS_FAILED(rv)) 
  {
    return;
  }

  nsAppShellProxy *appShellProxy = new nsAppShellProxy(installInfo->GetUIEventQueue(), appShell);

  // DO NOT CALL INIT!  appShellProxy->Initialize();
  
  nsString *aCID = new nsString("00000000-dead-beef-0000-000000000000");
  nsIWebShellWindow* newWindow;
  nsIURL* url;
  char* urlstr = "resource:/res/samples/xpinstallprogress.xul";
  NS_NewURL(&url, urlstr);


  appShellProxy->CreateDialogWindow(nsnull, 
                                    url, 
                                    *aCID, 
                                    newWindow,
                                    nsnull, 
                                    nsnull, 
                                    250, 
                                    125);
    
#endif
  
    RunInstall( (const char*) nsAutoCString( installInfo->GetLocalFile() ), 
                (const char*) nsAutoCString( installInfo->GetFlags()     ), 
                (const char*) nsAutoCString( installInfo->GetArguments() ),
                (const char*) nsAutoCString( installInfo->GetFromURL()   ));

#if 0
    appShellProxy->CloseTopLevelWindow(newWindow);

    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
    // After Install, we need to update the queue.
#endif

    nsISoftwareUpdate *softwareUpdate;

    rv = nsComponentManager::CreateInstance( kSoftwareUpdateCID, 
                                             nsnull,
                                             kISoftwareUpdateIID,
                                              (void**) &softwareUpdate);

    if (NS_FAILED(rv)) 
    {
        return ;
    }
    
    softwareUpdate->InstallJarCallBack();


}



///////////////////////////////////////////////////////////////////////////////////////////////
// Function name	: RunInstall
// Description	    : This is the main C entrypoint to run jar installers
// Return type		: PRInt32
// Argument         : const char* jarFile  - a native filepath to a jarfile to be run
// Argument         : const char* flags    - UNUSED
// Argument         : const char* args     - arguments passed into the javascript env
// Argument         : const char* fromURL  - a url string of where this file came from UNUSED
///////////////////////////////////////////////////////////////////////////////////////////////


extern "C" NS_EXPORT PRInt32 RunInstall(const char* jarFile, const char* flags, const char* args, const char* fromURL)
{

    char        *scriptBuffer;
    PRUint32    scriptLength;

    JSRuntime   *rt;
	JSContext   *cx;
    JSObject    *glob;

    nsresult rv = GetInstallScriptFromJarfile(jarFile, &scriptBuffer, &scriptLength);
    
    if (NS_FAILED(rv) || scriptBuffer == nsnull)
    {
        return rv;
    }

    rv = SetupInstallContext(jarFile, args, &rt, &cx, &glob);
    if (NS_FAILED(rv))
    {
        delete [] scriptBuffer;
        return rv;
    }
    


    
    // Go ahead and run!!
    jsval rval;

    JS_EvaluateScript(cx, 
                      glob,
		              scriptBuffer, 
                      scriptLength,
                      nsnull,
                      0,
		              &rval);
        
    
    
    delete [] scriptBuffer;
    
    JS_DestroyContext(cx);
	JS_DestroyRuntime(rt);
	            
    return rv;
}


