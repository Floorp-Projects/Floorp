/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=80: */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Douglas Turner <dougt@netscape.com>
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

#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"
#include "nsSoftwareUpdateIIDs.h"

#include "nsInstall.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsProxiedService.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"

#include "nspr.h"
#include "plstr.h"
#include "jsapi.h"

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
#include "nsStringEnumerator.h"

#include "nsIJAR.h"
#include "nsIPrincipal.h"

#include "nsIExtensionManager.h"

static NS_DEFINE_CID(kSoftwareUpdateCID,  NS_SoftwareUpdate_CID);

extern JSObject *InitXPInstallObjects(JSContext *jscontext, 
                                      nsIFile* jarfile, const PRUnichar* url, 
                                      const PRUnichar* args, PRUint32 flags, 
                                      CHROMEREG_IFACE* registry, 
                                      nsIZipReader* hZip);
extern nsresult InitInstallVersionClass(JSContext *jscontext, JSObject *global, void** prototype);
extern nsresult InitInstallTriggerGlobalClass(JSContext *jscontext, JSObject *global, void** prototype);

// Defined in this file:
PR_STATIC_CALLBACK(void) XPInstallErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);
static PRInt32  GetInstallScriptFromJarfile(nsIZipReader* hZip, char** scriptBuffer, PRUint32 *scriptLength);
static PRInt32  OpenAndValidateArchive(nsIZipReader* hZip, nsIFile* jarFile, nsIPrincipal* aPrincipal);

static nsresult SetupInstallContext(nsIZipReader* hZip, nsIFile* jarFile, const PRUnichar* url, const PRUnichar* args, 
                                    PRUint32 flags, CHROMEREG_IFACE* reg, JSRuntime *jsRT, JSContext **jsCX, JSObject **jsGlob);

extern "C" void RunInstallOnThread(void *data);


nsresult VerifySigning(nsIZipReader* hZip, nsIPrincipal* aPrincipal)
{
    if (!aPrincipal) 
        return NS_OK; // not signed, but not an error

    PRBool hasCert;
    aPrincipal->GetHasCertificate(&hasCert);
    if (!hasCert)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIJAR> jar(do_QueryInterface(hZip));
    if (!jar)
        return NS_ERROR_FAILURE;

    // See if the archive is signed at all first
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = jar->GetCertificatePrincipal(nsnull, getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal)
        return NS_ERROR_FAILURE;
    
    PRUint32 entryCount = 0;

    // first verify all files in the jar are also in the manifest.
    nsCOMPtr<nsIUTF8StringEnumerator> entries;
    rv = hZip->FindEntries(nsnull, getter_AddRefs(entries));
    if (NS_FAILED(rv))
        return rv;

    PRBool more;
    nsCAutoString name;
    while (NS_SUCCEEDED(entries->HasMore(&more)) && more)
    {
        rv = entries->GetNext(name);
        if (NS_FAILED(rv)) return rv;
        
        if (PL_strncasecmp("META-INF/", name.get(), 9) == 0)
            continue;

        // libjar creates fake entries for directories which are
        // not in the zip but do exist as part of the path of some
        // entry within the zip, e.g. foo/ in a zip containing
        // only foo/bar.txt -- skip those, because they shouldn't
        // be in the manifest
 
        // This is pretty inefficient, but maybe that's okay.
        nsCOMPtr<nsIZipEntry> entry;
        rv = hZip->GetEntry(name.get(), getter_AddRefs(entry));
        if (NS_FAILED(rv)) return rv;

        PRBool isSynthetic;
        rv = entry->GetIsSynthetic(&isSynthetic);
        if (NS_FAILED(rv)) return rv;
        if (isSynthetic)
            continue;

        // we only count the entries which are not in the meta-inf
        // directory and which are explicitly listed in the zip
        entryCount++;

        // Each entry must be signed
        rv = jar->GetCertificatePrincipal(name.get(), getter_AddRefs(principal));
        if (NS_FAILED(rv) || !principal) return NS_ERROR_FAILURE;

        PRBool equal;
        rv = principal->Equals(aPrincipal, &equal);
        if (NS_FAILED(rv) || !equal) return NS_ERROR_FAILURE;
    }

    // next verify all files in the manifest are in the archive.
    PRUint32 manifestEntryCount;
    rv = jar->GetManifestEntriesCount(&manifestEntryCount);
    if (NS_FAILED(rv))
        return rv;

    if (entryCount != manifestEntryCount)
        return NS_ERROR_FAILURE;  // some files were deleted from archive

    return NS_OK;
}

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
                               NS_ConvertASCIItoUTF16(report->filename).get(),
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

    // lets set up an eventQ so that our xpcom/proxies will not have to:
    nsCOMPtr<nsISoftwareUpdate> softwareUpdate =
             do_GetService(kSoftwareUpdateCID, &rv);

    if (NS_FAILED(rv))
    {
        NS_WARNING("shouldn't have RunInstall() if we can't get SoftwareUpdate");
        return;
    }

    nsCOMPtr<nsIXPIListener> listener;
    softwareUpdate->GetMasterListener(getter_AddRefs(listener));

    if(listener)
    {
        nsAutoString logMessage;
        if (report)
        {
            logMessage.AssignLiteral("Line: ");
            logMessage.AppendInt(report->lineno, 10);
            logMessage.AppendLiteral("\t");
            if (report->ucmessage)
                logMessage.Append( NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage) );
            else
                logMessage.AppendWithConversion( message );
        }
        else
            logMessage.AssignWithConversion( message );

        listener->OnLogComment( logMessage.get() );
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Function name    : OpenAndValidateArchive
// Description      : Opens install archive and validates contents
// Return type      : PRInt32
// Argument         : nsIZipReader* hZip       - the zip reader
// Argument         : nsIFile* jarFile         - the .xpi file
// Argument         : nsIPrincipal* aPrincipal - a principal, if any, displayed to the user 
//                    regarding the cert used to sign this install
///////////////////////////////////////////////////////////////////////////////////////////////

static PRInt32
OpenAndValidateArchive(nsIZipReader* hZip, nsIFile* jarFile, nsIPrincipal* aPrincipal)
{
    if (!jarFile)
        return nsInstall::DOWNLOAD_ERROR;

    nsCOMPtr<nsIFile> jFile;
    nsresult rv =jarFile->Clone(getter_AddRefs(jFile));
    if (NS_SUCCEEDED(rv))
        rv = hZip->Open(jFile);

    if (NS_FAILED(rv))
        return nsInstall::CANT_READ_ARCHIVE;

    // CRC check the integrity of all items in this archive
    rv = hZip->Test(nsnull);
    if (NS_FAILED(rv))
    {
        NS_WARNING("CRC check of archive failed!");
        return nsInstall::CANT_READ_ARCHIVE;
    }

    rv = VerifySigning(hZip, aPrincipal);
    if (NS_FAILED(rv))
    {
        NS_WARNING("Signing check of archive failed!");
        return nsInstall::INVALID_SIGNATURE;
    }
 
    return nsInstall::SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Function name    : GetInstallScriptFromJarfile
// Description      : Extracts and reads in a install.js file from a passed jar file.
// Return type      : static PRInt32
// Argument         : char** scriptBuffer     - must be deleted via delete []
// Argument         : PRUint32 *scriptLength
///////////////////////////////////////////////////////////////////////////////////////////////

static PRInt32
GetInstallScriptFromJarfile(nsIZipReader* hZip, char** scriptBuffer, PRUint32 *scriptLength)
{
    PRInt32 result = NS_OK;

    *scriptBuffer = nsnull;
    *scriptLength = 0;

    // Extract the install.js file.
    nsCOMPtr<nsIInputStream> instream;
    nsresult rv = hZip->GetInputStream("install.js", getter_AddRefs(instream));
    if ( NS_SUCCEEDED(rv) )
    {
        // Read it into a buffer
        char* buffer;
        PRUint32 bufferLength;
        PRUint32 readLength;
        result = nsInstall::CANT_READ_ARCHIVE;

        rv = instream->Available(&bufferLength);
        if (NS_SUCCEEDED(rv))
        {
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
        }
        instream->Close();
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
                                    CHROMEREG_IFACE* reg,
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

    JS_BeginRequest(cx);
    glob = InitXPInstallObjects(cx, jarFile, url, args, flags, reg, hZip);
    if (!glob)
    {
        JS_DestroyContext(cx);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Init standard classes
    JS_InitStandardClasses(cx, glob);

    // Add our Install class to this context
    InitInstallVersionClass(cx, glob, nsnull);
    InitInstallTriggerGlobalClass(cx, glob, nsnull);
    JS_EndRequest(cx);
    *jsCX   = cx;
    *jsGlob = glob;

    return NS_OK;
}




///////////////////////////////////////////////////////////////////////////////////////////////
// Function name    : RunInstall
// Description      : Creates our Install Thread.
// Return type      : PRInt32
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
// Function name    : RunInstallOnThread
// Description      : called by starting thread.  It directly calls the C api for xpinstall,
//                  : and once that returns, it calls the completion routine to notify installation
//                  : completion.
// Return type      : extern "C"
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

    static NS_DEFINE_IID(kZipReaderCID,  NS_ZIPREADER_CID);

    nsresult rv;
    nsCOMPtr<nsIZipReader> hZip = do_CreateInstance(kZipReaderCID, &rv);
    if (NS_FAILED(rv))
        return;

    // we will plan on sending a failure status back from here unless we
    // find positive acknowledgement that the script sent the status
    PRInt32     finalStatus;

    nsCOMPtr<nsIXPIListener> listener;

    nsCOMPtr<nsISoftwareUpdate> softwareUpdate =
             do_GetService(kSoftwareUpdateCID, &rv);

    if (NS_FAILED(rv))
    {
        NS_WARNING("shouldn't have RunInstall() if we can't get SoftwareUpdate");
        return;
    }

    softwareUpdate->SetActiveListener( installInfo->GetListener() );
    softwareUpdate->GetMasterListener(getter_AddRefs(listener));

    if(listener)
        listener->OnInstallStart( installInfo->GetURL() );

    nsCOMPtr<nsIFile> jarpath = installInfo->GetFile();

    finalStatus = OpenAndValidateArchive( hZip,
                                          jarpath,
                                          installInfo->mPrincipal);

    if (finalStatus == nsInstall::SUCCESS)
    {
#ifdef MOZ_XUL_APP
        if (NS_SUCCEEDED(hZip->Test("install.rdf")) && !(nsSoftwareUpdate::GetProgramDirectory()))
        {
            hZip->Close();
            // appears to be an Extension Manager install
            nsIExtensionManager* em = installInfo->GetExtensionManager();
            if (em)
            {
                rv = em->InstallItemFromFile(jarpath, 
                                             NS_INSTALL_LOCATION_APPPROFILE);
                if (NS_FAILED(rv))
                    finalStatus = nsInstall::EXECUTION_ERROR;
            } else {
                finalStatus = nsInstall::UNEXPECTED_ERROR;
            }
            // If install.rdf exists, but the install failed, we don't want
            // to try an install.js install.
        } else
#endif
        {
            // If we're the suite, or there is no install.rdf,
            // try original XPInstall
            finalStatus = GetInstallScriptFromJarfile( hZip,
                                                       &scriptBuffer,
                                                       &scriptLength);
            if ( finalStatus == NS_OK && scriptBuffer )
            {
                // create our runtime
                rt = JS_NewRuntime(4L * 1024L * 1024L);

                rv = SetupInstallContext( hZip, jarpath,
                                          installInfo->GetURL(),
                                          installInfo->GetArguments(),
                                          installInfo->GetFlags(),
                                          installInfo->GetChromeRegistry(),
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

                        if(JS_GetProperty(cx, glob, "_installedFiles", &installedFiles) &&
                          JSVAL_TO_BOOLEAN(installedFiles))
                        {
                            // install items remain in queue, must clean up!
                            nsInstall *a = (nsInstall*)JS_GetPrivate(cx, glob);
                            a->InternalAbort(nsInstall::MALFORMED_INSTALL);
                        }

                        jsval sent;
                        if ( JS_GetProperty( cx, glob, "_finalStatus", &sent ) )
                            finalStatus = JSVAL_TO_INT(sent);
                        else
                            finalStatus = nsInstall::UNEXPECTED_ERROR;
                    }
                    JS_EndRequest(cx); //Decrement JS thread counter
                    JS_DestroyContextMaybeGC(cx);
                }
                else
                {
                    // couldn't initialize install context
                    finalStatus = nsInstall::UNEXPECTED_ERROR;
                }

                // Clean up after ourselves.
                JS_DestroyRuntime(rt);
            }
        }
        // force zip archive closed before other cleanup
        hZip = 0;
    }

    if(listener)
        listener->OnInstallDone( installInfo->GetURL(), finalStatus );

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
        listener->OnInstallStart(info->GetURL());

    // make sure we've got a chrome registry -- can't proceed if not
    CHROMEREG_IFACE* reg = info->GetChromeRegistry();
    NS_ASSERTION(reg, "We shouldn't get here without a chrome registry.");

    if (reg)
    {
#ifdef MOZ_XUL_APP
        if (info->GetType() == CHROME_SKIN) {
            static NS_DEFINE_CID(kZipReaderCID,  NS_ZIPREADER_CID);
            nsCOMPtr<nsIZipReader> hZip = do_CreateInstance(kZipReaderCID, &rv);
            if (NS_SUCCEEDED(rv) && hZip) {
                rv = hZip->Open(info->GetFile());
                if (NS_SUCCEEDED(rv))
                {
                    rv = hZip->Test("install.rdf");
                    nsIExtensionManager* em = info->GetExtensionManager();
                    if (NS_SUCCEEDED(rv) && em) {
                        rv = em->InstallItemFromFile(info->GetFile(), 
                                                     NS_INSTALL_LOCATION_APPPROFILE);
                    }
                }
                hZip->Close();
            }
            // Extension Manager copies the theme .jar file to 
            // a different location, so remove the temporary file.
            info->GetFile()->Remove(PR_FALSE);
        }
#else
        PRBool isSkin    = (info->GetType() & CHROME_SKIN);
        PRBool isLocale  = (info->GetType() & CHROME_LOCALE);
        PRBool isContent = (info->GetType() & CHROME_CONTENT);
        PRBool selected  = (info->GetFlags() != 0);

        const nsCString& spec = info->GetFileJARSpec();

        if ( isContent )
            rv = reg->InstallPackage(spec.get(), PR_TRUE);

        if ( isSkin )
        {
            rv = reg->InstallSkin(spec.get(), PR_TRUE, PR_FALSE);
                
            if (NS_SUCCEEDED(rv) && selected)
            {
                NS_ConvertUTF16toUTF8 utf8Args(info->GetArguments());
                rv = reg->SelectSkin(utf8Args, PR_TRUE);
            }
        }

        if ( isLocale )
        {
            rv = reg->InstallLocale(spec.get(), PR_TRUE);

            if (NS_SUCCEEDED(rv) && selected)
            {
                NS_ConvertUTF16toUTF8 utf8Args(info->GetArguments());
                rv = reg->SelectLocale(utf8Args, PR_TRUE);
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
#endif
    }

    if (listener)
        listener->OnInstallDone(info->GetURL(), nsInstall::SUCCESS);

    delete info;
}
