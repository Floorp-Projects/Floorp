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

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsSpecialSystemDirectory.h"
#include "nsProxiedService.h"
#include "nsFileStream.h"
#include "nsIURI.h"
#include "nsIFileChannel.h"
#include "nsNetUtil.h"

#include "nspr.h"
#include "jsapi.h"

#include "nsIEventQueueService.h"
#include "nsIEnumerator.h"
#include "nsIZipReader.h"
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsILocalFile.h"
#include "nsIChromeRegistry.h"
#include "nsInstallTrigger.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

static NS_DEFINE_CID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

extern JSObject *InitXPInstallObjects(JSContext *jscontext, JSObject *global, nsIFile* jarfile, const PRUnichar* url, const PRUnichar* args, PRUint32 flags, nsIChromeRegistry* reg, nsIDOMWindowInternal* aParent, nsIZipReader* hZip);
extern nsresult InitInstallVersionClass(JSContext *jscontext, JSObject *global, void** prototype);
extern nsresult InitInstallTriggerGlobalClass(JSContext *jscontext, JSObject *global, void** prototype);

// Defined in this file:
PR_STATIC_CALLBACK(void) XPInstallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);
static PRInt32  GetInstallScriptFromJarfile(nsIZipReader* hZip, nsIFile* jarFile, char** scriptBuffer, PRUint32 *scriptLength);
static nsresult SetupInstallContext(nsIZipReader* hZip, nsIFile* jarFile, const PRUnichar* url, const PRUnichar* args, PRUint32 flags, nsIChromeRegistry* reg, nsIDOMWindowInternal* aParent, JSRuntime *jsRT, JSContext **jsCX, JSObject **jsGlob);

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
        (do_GetService("@mozilla.org/consoleservice;1"));

    /*
     * Make an nsIScriptError, populate it with information from this
     * error, then log it with the console service.
     */
    nsCOMPtr<nsIScriptError>
        errorObject(do_CreateInstance("@mozilla.org/scripterror;1"));
    
    if (consoleService != nsnull && errorObject != nsnull && report != nsnull) {
        /*
         * Got an error object; prepare appropriate-width versions of
         * various arguments to it.
         */

        PRUint32 column = report->uctokenptr - report->uclinebuf;

        rv = errorObject->Init(NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage),
                               NS_ConvertASCIItoUCS2(report->filename).get(),
                               NS_REINTERPRET_CAST(const PRUnichar*, report->uclinebuf),
                               report->lineno, column, report->flags,
                               "XPInstall JavaScript");
        if (NS_SUCCEEDED(rv)) {
            rv = consoleService->LogMessage(errorObject);
            if (NS_SUCCEEDED(rv)) {
              // We're done!
              // For now, always also print out the error to stderr.
              // return;
            }
        }
    }
    
    if (!report)
        return;

    nsIXPIListener *listener;

    // lets set up an eventQ so that our xpcom/proxies will not have to:
    nsCOMPtr<nsISoftwareUpdate> softwareUpdate = 
             do_GetService(kSoftwareUpdateCID, &rv);

    if (!NS_SUCCEEDED(rv))

    {
        NS_WARNING("shouldn't have RunInstall() if we can't get SoftwareUpdate");
        return;
    }

    softwareUpdate->GetMasterListener(&listener);
    
    if(listener)
    {
        nsAutoString logMessage;
        logMessage.AssignWithConversion("Line: ");
        logMessage.AppendInt(report->lineno, 10);
        logMessage.AppendWithConversion("\t");
        if (report->ucmessage)
            logMessage.Append( NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage) ); 
        else
            logMessage.AppendWithConversion( message ); 

        listener->LogComment( logMessage.get() );
    }
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
    //NS_NewLocalFile(jarFile, PR_TRUE, getter_AddRefs(jFile));
    if (NS_SUCCEEDED(rv))
      rv = hZip->Init(jFile);

    if (NS_FAILED(rv))
        return nsInstall::CANT_READ_ARCHIVE;

    rv = hZip->Open();
    if (NS_FAILED(rv))
        return nsInstall::CANT_READ_ARCHIVE;

    // CRC check the integrity of all items in this archive
    rv = hZip->Test(nsnull);
    if (NS_FAILED(rv))
    {
        NS_ASSERTION(0, "CRC check of archive failed!");
        return nsInstall::CANT_READ_ARCHIVE;
    }

    // Extract the install.js file to the temporary directory
    nsSpecialSystemDirectory installJSFileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    installJSFileSpec += "install.js";
    installJSFileSpec.MakeUnique();

    // Extract the install.js file.
    nsCOMPtr<nsILocalFile> iFile;
    rv = NS_NewLocalFile(installJSFileSpec, PR_TRUE, getter_AddRefs(iFile));
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
// Argument         : nsIChromeRegistry* - the chrome registry to run with.
// Argument         : JSRuntime *jsRT    - A valid JS Runtime
// Argument         : JSContext **jsCX   - Created context, destroy via JS_DestroyContext
// Argument         : JSObject **jsGlob  - created global object
///////////////////////////////////////////////////////////////////////////////////////////////
static nsresult SetupInstallContext(nsIZipReader* hZip,
                                    nsIFile* jarFile,
                                    const PRUnichar* url,
                                    const PRUnichar* args,
                                    PRUint32 flags,
                                    nsIChromeRegistry* reg,
                                    nsIDOMWindowInternal* aParent,
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


    glob = InitXPInstallObjects(cx, nsnull, jarFile, url, args, flags, reg, aParent, hZip);
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

    nsCOMPtr<nsIXPIListener> listener;

    // lets set up an eventQ so that our xpcom/proxies will not have to:
    nsCOMPtr<nsIEventQueue> eventQ;
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) 
    {   
        eventQService->CreateMonitoredThreadEventQueue();
        eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    }

    nsCOMPtr<nsISoftwareUpdate> softwareUpdate = 
             do_GetService(kSoftwareUpdateCID, &rv);

    if (!NS_SUCCEEDED(rv))
    {
        NS_WARNING("shouldn't have RunInstall() if we can't get SoftwareUpdate");
        return;
    }

    softwareUpdate->SetActiveListener( installInfo->GetListener() );
    softwareUpdate->GetMasterListener(getter_AddRefs(listener));
    
    if(listener)
        listener->BeforeJavascriptEvaluation( installInfo->GetURL() );
    
    nsCOMPtr<nsIFile> jarpath = installInfo->GetFile();
    if (NS_SUCCEEDED(rv))
    {
        finalStatus = GetInstallScriptFromJarfile( hZip,
                                                   jarpath,
                                                   &scriptBuffer, 
                                                   &scriptLength);

        if ( finalStatus == NS_OK && scriptBuffer )
        {
            PRBool ownRuntime = PR_FALSE;

            nsCOMPtr<nsIJSRuntimeService> rtsvc = 
                     do_GetService("@mozilla.org/js/xpc/RuntimeService;1", &rv);
            if(NS_FAILED(rv) || NS_FAILED(rtsvc->GetRuntime(&rt)))
            {
                // service not available (wizard context?)
                // create our own runtime
                ownRuntime = PR_TRUE;
                rt = JS_Init(4L * 1024L * 1024L);
            }

            rv = SetupInstallContext( hZip, jarpath,
                                      installInfo->GetURL(),
                                      installInfo->GetArguments(),
                                      installInfo->GetFlags(),
                                      installInfo->GetChromeRegistry(),
                                      installInfo->GetParentDOMWindow(),
                                      rt, &cx, &glob);

            if (NS_SUCCEEDED(rv))
            {
                // Go ahead and run!!
                jsval rval;
                jsval installedFiles;
                JS_BeginRequest(cx); //Increment JS thread counter associated 
                                     //with this context
                PRBool ok = JS_EvaluateScript(  cx,
                                                glob,
                                                scriptBuffer,
                                                scriptLength,
                                                nsnull,
                                                0,
                                                &rval);


                if(!ok)
                {
                    // problem compiling or running script -- a true SCRIPT_ERROR
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
                    // check to make sure the script sent back a status -- if
                    // not the install may have been syntactically correct but
                    // left the init/(perform|cancel) transaction open

                    jsval sent;

                    if(JS_GetProperty(cx, glob, "_installedFiles", &installedFiles) &&
                       JSVAL_TO_BOOLEAN(installedFiles))
                    {
                      nsInstall *a = (nsInstall*)JS_GetPrivate(cx, glob);
                      a->InternalAbort(nsInstall::MALFORMED_INSTALL);
                    }

                    if ( JS_GetProperty( cx, glob, "_statusSent", &sent ) &&
                         JSVAL_TO_BOOLEAN(sent) )
                        sendStatus = PR_FALSE;
                    else
                        finalStatus = nsInstall::MALFORMED_INSTALL;
                }
                JS_EndRequest(cx); //Decrement JS thread counter
                JS_DestroyContextMaybeGC(cx);
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

    if(listener) 
    {
        if ( sendStatus )
            listener->FinalStatus( installInfo->GetURL(), finalStatus );

        listener->AfterJavascriptEvaluation( installInfo->GetURL() );
    }

    if (scriptBuffer) delete [] scriptBuffer;

    softwareUpdate->SetActiveListener(0);
    softwareUpdate->InstallJarCallBack();
}


//-----------------------------------------------------------------------------
// RunChromeInstallOnThread
//
// Performs the guts of a chrome install on its own thread
//
// XXX: need to return errors/status somehow. What feedback will a user want?
// How do we get it there? Maybe just alerts on errors, could also dump to
// the new console service.
//-----------------------------------------------------------------------------
extern "C" void RunChromeInstallOnThread(void *data)
{
    nsresult rv;

    NS_ASSERTION(data, "No nsInstallInfo passed to Chrome Install");
    nsInstallInfo *info = (nsInstallInfo*)data;
    nsIXPIListener* listener = info->GetListener();

    if (listener)
        listener->BeforeJavascriptEvaluation(info->GetURL());

    // make sure we've got a chrome registry -- can't proceed if not
    nsIChromeRegistry* reg = info->GetChromeRegistry();
    if (reg)
    {
        // build up jar: URL
        nsCString spec;
        spec.SetCapacity(200);
        spec = "jar:";

        nsCOMPtr<nsIURI> pURL;
        rv = NS_NewURI(getter_AddRefs(pURL), "file:");
        if (NS_SUCCEEDED(rv)) 
        {
            nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(pURL);
            if (fileURL)
                rv = fileURL->SetFile(info->GetFile());
            else
                rv = NS_ERROR_NO_INTERFACE;

            if (NS_SUCCEEDED(rv))
            {
                nsXPIDLCString localURL;
                rv = fileURL->GetSpec(getter_Copies(localURL));
                spec.Append(localURL);
                spec.Append("!/");
            }
        }

        // Now register the new chrome
        if (NS_SUCCEEDED(rv))
        {
            PRBool isSkin    = (info->GetType() & CHROME_SKIN);
            PRBool isLocale  = (info->GetType() & CHROME_LOCALE);
            PRBool isContent = (info->GetType() & CHROME_CONTENT);
            PRBool selected  = (info->GetFlags() != 0);
 
            if ( isContent )
            {
                rv = reg->InstallPackage(spec.get(), PR_TRUE);
            }

            if ( isSkin )
            {
              rv = reg->InstallSkin(spec.get(), PR_TRUE, PR_FALSE);
                if (NS_SUCCEEDED(rv) && selected)
                {
                    rv = reg->SelectSkin(info->GetArguments(), PR_TRUE);
                }
            }

            if ( isLocale ) 
            {
                rv = reg->InstallLocale(spec.get(), PR_TRUE);
                if (NS_SUCCEEDED(rv) && selected)
                {
                    rv = reg->SelectLocale(info->GetArguments(), PR_TRUE);
                }
            }

            // now that all types are registered try to activate
            if ( isSkin && selected )
                reg->RefreshSkins();
 
#ifdef RELOAD_CHROME_WORKS
// XXX ReloadChrome() crashes right now
            if ( isContent || (isLocale && selected) )
                reg->ReloadChrome();
#endif
        }
    }

    if (listener)
        listener->AfterJavascriptEvaluation(info->GetURL());

    delete info;
}
