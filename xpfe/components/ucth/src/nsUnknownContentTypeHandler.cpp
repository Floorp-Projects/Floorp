/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsIUnkContentTypeHandler.h"

#include "nsIAppShellComponentImpl.h"

#include "nsString.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIStreamObserver.h"

// {42770B50-03E9-11d3-8068-00600811A9C3}
#define NS_UNKNOWNCONTENTTYPEHANDLER_CID \
    { 0x42770b50, 0x3e9, 0x11d3, { 0x80, 0x68, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

class nsUnknownContentTypeHandler : public nsIUnknownContentTypeHandler,
                                    public nsAppShellComponentImpl {
public:
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_UNKNOWNCONTENTTYPEHANDLER_CID );

    // ctor/dtor
    nsUnknownContentTypeHandler() {
        NS_INIT_REFCNT();
    }
    virtual ~nsUnknownContentTypeHandler() {
    }

    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIAppShellComponent interface functions.
    NS_DECL_NSIAPPSHELLCOMPONENT

    // This class implements the nsIUnknownContentTypeHandler interface functions.
    NS_DECL_NSIUNKNOWNCONTENTTYPEHANDLER

private:
    nsInstanceCounter            mInstanceCounter;
}; // nsUnknownContentTypeHandler

// HandleUnknownContentType (from nsIUnknownContentTypeHandler) implementation.
NS_IMETHODIMP
nsUnknownContentTypeHandler::HandleUnknownContentType( nsIChannel *aChannel,
                                                       const char *aContentType,
                                                       nsIDOMWindow *aWindow ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsISupports> channel;

    if ( aChannel ) {
        // Need root nsISupports for later JS_PushArguments call.
        channel = do_QueryInterface( aChannel );

        // Cancel input channel now.
        rv = aChannel->Cancel(NS_BINDING_ABORTED);
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Cancel failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    if ( NS_SUCCEEDED( rv ) && channel && aContentType && aWindow ) {
        // Open "Unknown content type" dialog.
        // We pass in the channel and the content type.
        // Note that the "parent" browser window will be window.opener within the
        // new dialog.
    
        // Get JS context from parent window.
        nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( aWindow, &rv );
        if ( NS_SUCCEEDED( rv ) && sgo ) {
            nsCOMPtr<nsIScriptContext> context;
            sgo->GetContext( getter_AddRefs( context ) );
            if ( context ) {
                JSContext *jsContext = (JSContext*)context->GetNativeContext();
                if ( jsContext ) {
                    void *stackPtr;
                    jsval *argv = JS_PushArguments( jsContext,
                                                    &stackPtr,
                                                    "sss%ips",
                                                    "chrome://global/content/unknownContent.xul",
                                                    "_blank",
                                                    "chrome",
                                                    (const nsIID*)(&NS_GET_IID(nsIChannel)),
                                                    (nsISupports*)channel.get(),
                                                    aContentType );
                    if ( argv ) {
                        nsCOMPtr<nsIDOMWindow> newWindow;
                        rv = aWindow->OpenDialog( jsContext, argv, 5, getter_AddRefs( newWindow ) );
                        if ( NS_FAILED( rv ) ) {
                            DEBUG_PRINTF( PR_STDOUT, "%s %d: OpenDialog failed, rv=0x%08X\n",
                                          (char*)__FILE__, (int)__LINE__, (int)rv );
                        }
                        JS_PopArguments( jsContext, stackPtr );
                    } else {
                        DEBUG_PRINTF( PR_STDOUT, "%s %d: JS_PushArguments failed\n",
                                      (char*)__FILE__, (int)__LINE__ );
                        rv = NS_ERROR_FAILURE;
                    }
                } else {
                    DEBUG_PRINTF( PR_STDOUT, "%s %d: GetNativeContext failed\n",
                                  (char*)__FILE__, (int)__LINE__ );
                    rv = NS_ERROR_FAILURE;
                }
            } else {
                DEBUG_PRINTF( PR_STDOUT, "%s %d: GetContext failed\n",
                              (char*)__FILE__, (int)__LINE__ );
                rv = NS_ERROR_FAILURE;
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: QueryInterface (for nsIScriptGlobalObject) failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        // If no error recorded so far, set one now.
        if ( NS_SUCCEEDED( rv ) ) {
            rv = NS_ERROR_NULL_POINTER;
        }
    }

    return rv;
}

// Generate base nsIAppShellComponent implementation.
NS_IMPL_IAPPSHELLCOMPONENT( nsUnknownContentTypeHandler,
                            nsIUnknownContentTypeHandler,
                            NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID,
                            0 )
