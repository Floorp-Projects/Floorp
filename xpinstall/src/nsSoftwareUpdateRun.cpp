/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 */

#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"
#include "nsSoftwareUpdateIIDs.h"

#include "nsInstall.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

#include "nsSpecialSystemDirectory.h"
#include "nsFileStream.h"

#include "nspr.h"
#include "jsapi.h"

#include "nsIEventQueueService.h"
#include "nsIEnumerator.h"
#include "nsIZipReader.h"
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"

#include "nsIEventQueueService.h"
#include "nsILocalFile.h"

#include "nsIConsoleService.h"
#include "nsIScriptError.h"

static NS_DEFINE_CID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

extern JSObject *InitXPInstallObjects(JSContext *jscontext, JSObject *global, nsIFile* jarfile, const PRUnichar* url, const PRUnichar* args, PRUint32 flags, nsIZipReader* hZip);
extern nsresult InitInstallVersionClass(JSContext *jscontext, JSObject *global, void** prototype);
extern nsresult InitInstallTriggerGlobalClass(JSContext *jscontext, JSObject *global, void** prototype);

// Defined in this file:
static void     XPInstallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);
static PRInt32  GetInstallScriptFromJarfile(nsIZipReader* hZip, nsIFile* jarFile, char** scriptBuffer, PRUint32 *scriptLength);
static nsresult SetupInstallContext(nsIZipReader* hZip, nsIFile* jarFile, const PRUnichar* url, const PRUnichar* args, PRUint32 flags, JSRuntime *jsRT, JSContext **jsCX, JSObject **jsGlob);

extern "C" void RunInstallOnThread(void *data);


///////////////////////////////////////////////////////////////////////////////////////////////
// Function name    : XPInstallErrorReporter
// Description      : Prints error message to stdout
// Return type      : void
// Argument         : JSContext *cx
// Argument         : const char *message
// Argument         : JSErrorReport *report
///////////////////////////////////////////////////////////////////////////////////////////////
static void
XPInstallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    nsresult rv;

    /* Use the console service to register the error. */
    nsCOMPtr<nsIConsoleService> consoleService
        (do_GetService("mozilla.consoleservice.1"));

    /*
     * Make an nsIScriptError, populate it with information from this
     * error, then log it with the console service.
     */
    nsCOMPtr<nsIScriptError>
        errorObject(do_CreateInstance("mozilla.scripterror.1"));
    
    if (consoleService != nsnull && errorObject != nsnull && report != nsnull) {
        /*
         * Got an error object; prepare appropriate-width versions of
         * various arguments to it.
         */
        nsAutoString fileUni;
        fileUni.AssignWithConversion(report->filename);

        const PRUnichar *newFileUni = fileUni.ToNewUnicode();
        
        PRUint32 column = report->uctokenptr - report->uclinebuf;

        rv = errorObject->Init(report->ucmessage, newFileUni, report->uclinebuf,
                               report->lineno, column, report->flags,
                               "XP Install JavaScript");
        nsAllocator::Free((void *)newFileUni);
        if (NS_SUCCEEDED(rv)) {
            rv = consoleService->LogMessage(errorObject);
            if (NS_SUCCEEDED(rv)) {
              // We're done!
              // For now, always also print out the error to stderr.
              // return;
            }
        }
    }

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
// Function name    : GetInstallScriptFromJarfile
// Description      : Extracts and reads in a install.js file from a passed jar file.
// Return type      : static PRInt32 
// Argument         : const char* jarFile     - **NSPR** filepath
// Argument         : char** scriptBuffer     - must be deleted via delete []
// Argument         : PRUint32 *scriptLength
///////////////////////////////////////////////////////////////////////////////////////////////

static PRInt32
GetInstallScriptFromJarfile(nsIZipReader* hZip, nsIFile* jarFile, char** scriptBuffer, PRUint32 *scriptLength)
{
    PRInt32 result = NS_OK;
    
    *scriptBuffer = nsnull;
    *scriptLength = 0;

    nsIFile* jFile;
    nsresult rv =jarFile->Clone(&jFile);
    //NS_NewLocalFile(jarFile, getter_AddRefs(jFile));
    if (NS_SUCCEEDED(rv))
      rv = hZip->Init(jFile);

    if (NS_FAILED(rv))
        return nsInstall::CANT_READ_ARCHIVE;

    rv = hZip->Open();
    if (NS_FAILED(rv))
        return nsInstall::CANT_READ_ARCHIVE;

    // Extract the install.js file to the temporary directory
    nsSpecialSystemDirectory installJSFileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    installJSFileSpec += "install.js";
    installJSFileSpec.MakeUnique();

    // Extract the install.js file.
    nsCOMPtr<nsILocalFile> iFile;
    rv = NS_NewLocalFile(installJSFileSpec, getter_AddRefs(iFile));
    if (NS_SUCCEEDED(rv))
      rv = hZip->Extract("install.js", iFile);
    if ( NS_SUCCEEDED(rv) )
    {
        // Read it into a buffer
        char* buffer;
        PRUint32 bufferLength;
        PRUint32 readLength;
        result = nsInstall::CANT_READ_ARCHIVE;

        nsInputFileStream fileStream(installJSFileSpec);
        nsCOMPtr<nsIInputStream> instream = fileStream.GetIStream();

        if ( instream )
        {
            instream->Available(&bufferLength);
            buffer = new char[bufferLength + 1];

            if (buffer != nsnull)
            {
                rv = instream->Read(buffer, bufferLength, &readLength);

                if (NS_SUCCEEDED(rv) && readLength > 0)
                {
                    *scriptBuffer = buffer;
                    *scriptLength = readLength;
                    result = NS_OK;
                }
                else
                {
                    delete [] buffer;
                }
            }

            fileStream.close();
        }

        installJSFileSpec.Delete(PR_FALSE);
    }
    else
    {
        result = nsInstall::NO_INSTALL_SCRIPT;
    }

    return result;   
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Function name    : SetupInstallContext
// Description      : Creates a Javascript context and adds our xpinstall objects to it.
// Return type      : static nsresult
// Argument         : nsIZipReader hZip - the handle to the open archive file
// Argument         : const char* jarFile - native filepath to where jar exists on disk 
// Argument         : const PRUnichar* url  - URL of where this package came from
// Argument         : const PRUnichar* args    - any arguments passed into the javascript context
// Argument         : PRUint32 flags   - bitmask of flags passed in
// Argument         : JSRuntime *jsRT    - A valid JS Runtime
// Argument         : JSContext **jsCX   - Created context, destroy via JS_DestroyContext
// Argument         : JSObject **jsGlob  - created global object
///////////////////////////////////////////////////////////////////////////////////////////////
static nsresult SetupInstallContext(nsIZipReader* hZip,
                                    nsIFile* jarFile,
                                    const PRUnichar* url,
                                    const PRUnichar* args,
                                    PRUint32 flags,
                                    JSRuntime *rt, 
                                    JSContext **jsCX, 
                                    JSObject **jsGlob)
{
    JSContext   *cx;
    JSObject    *glob;
    
    *jsCX   = nsnull;
    *jsGlob = nsnull;

    if (!rt) 
        return NS_ERROR_OUT_OF_MEMORY;

    cx = JS_NewContext(rt, 8192);
    if (!cx)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    JS_SetErrorReporter(cx, XPInstallErrorReporter);


    glob = InitXPInstallObjects(cx, nsnull, jarFile, url, args, flags, hZip);
    // Init standard classes
    JS_InitStandardClasses(cx, glob);

    // Add our Install class to this context
    InitInstallVersionClass(cx, glob, nsnull);
    InitInstallTriggerGlobalClass(cx, glob, nsnull);

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
    if (installInfo->GetFlags() & XPI_NO_NEW_THREAD)
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
    
    char        *scriptBuffer = nsnull;
    PRUint32    scriptLength;

    JSRuntime   *rt;
    JSContext   *cx;
    JSObject    *glob;

    nsCOMPtr<nsIZipReader> hZip;

    static NS_DEFINE_IID(kIZipReaderIID, NS_IZIPREADER_IID);
    static NS_DEFINE_IID(kZipReaderCID,  NS_ZIPREADER_CID);
    nsresult rv = nsComponentManager::CreateInstance(kZipReaderCID, nsnull, kIZipReaderIID, 
                                                     getter_AddRefs(hZip));

    if (NS_FAILED(rv))
        return;

    // we will plan on sending a failure status back from here unless we
    // find positive acknowledgement that the script sent the status
    PRInt32     finalStatus;
    PRBool      sendStatus = PR_TRUE;

    nsIXPINotifier *notifier;

    // lets set up an eventQ so that our xpcom/proxies will not have to:
    nsCOMPtr<nsIEventQueue> eventQ;
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) 
    {   
        eventQService->CreateThreadEventQueue();
        eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    }

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

    nsCOMPtr<nsIFile> jarpath;
    rv = installInfo->GetLocalFile(getter_AddRefs(jarpath));
    if (NS_SUCCEEDED(rv))
    {
        finalStatus = GetInstallScriptFromJarfile( hZip,
                                                   jarpath,
                                                   &scriptBuffer, 
                                                   &scriptLength);

        if ( finalStatus == NS_OK && scriptBuffer )
        {
            PRBool ownRuntime = PR_FALSE;

            NS_WITH_SERVICE(nsIJSRuntimeService, rtsvc, "nsJSRuntimeService", &rv);
            if(NS_FAILED(rv) || NS_FAILED(rtsvc->GetRuntime(&rt)))
            {
                // service not available (wizard context?)
                // create our own runtime
                ownRuntime = PR_TRUE;
                rt = JS_Init(4L * 1024L * 1024L);
            }

            rv = SetupInstallContext( hZip, jarpath,
                                      url.GetUnicode(),
                                      args.GetUnicode(),
                                      installInfo->GetFlags(),
                                      rt, &cx, &glob);

            if (NS_SUCCEEDED(rv))
            {
                // Go ahead and run!!
                jsval rval;
                jsval installedFiles;

                PRBool ok = JS_EvaluateScript(  cx,
                                                glob,
                                                scriptBuffer,
                                                scriptLength,
                                                nsnull,
                                                0,
                                                &rval);


                if(!ok)
                {
                    // problem compiling or running script
                    if(JS_GetProperty(cx, glob, "_installedFiles", &installedFiles) &&
                       JSVAL_TO_BOOLEAN(installedFiles))
                    {
                        nsInstall *a = (nsInstall*)JS_GetPrivate(cx, glob);
                        a->InternalAbort(nsInstall::SCRIPT_ERROR);
                    }

                    finalStatus = nsInstall::SCRIPT_ERROR;
                }
                else
                {
                    // check to make sure the script sent back a status
                    jsval sent;

                    if(JS_GetProperty(cx, glob, "_installedFiles", &installedFiles) &&
                       JSVAL_TO_BOOLEAN(installedFiles))
                    {
                      nsInstall *a = (nsInstall*)JS_GetPrivate(cx, glob);
                      a->InternalAbort(nsInstall::SCRIPT_ERROR);
                    }

                    if ( JS_GetProperty( cx, glob, "_statusSent", &sent ) &&
                         JSVAL_TO_BOOLEAN(sent) )
                        sendStatus = PR_FALSE;
                    else
                        finalStatus = nsInstall::SCRIPT_ERROR;
                }

                JS_DestroyContext(cx);
            }
            else
            {
                // couldn't initialize install context
                finalStatus = nsInstall::UNEXPECTED_ERROR;
            }

            // clean up Runtime if we created it ourselves
            if ( ownRuntime ) 
                JS_DestroyRuntime(rt);
        }
        // force zip archive closed before other cleanup
        hZip = 0;
    }
    else 
    {
        // no path to local jar archive
        finalStatus = nsInstall::DOWNLOAD_ERROR;
    }

    if(notifier) 
    {
        if ( sendStatus )
            notifier->FinalStatus( url.GetUnicode(), finalStatus );

        notifier->AfterJavascriptEvaluation( url.GetUnicode() );
    }

    if (scriptBuffer) delete [] scriptBuffer;

    softwareUpdate->SetActiveNotifier(0);
    softwareUpdate->InstallJarCallBack();
}
