/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIUnkContentTypeHandler.h"

#include "nsIAppShellComponentImpl.h"

#include "nsString.h"
#include "nsIURL.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"

#ifdef NECKO
#include "nsIChannel.h"
#endif

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
    NS_DECL_IAPPSHELLCOMPONENT

    // This class implements the nsIUnknownContentTypeHandler interface functions.
    NS_DECL_IUNKNOWNCONTENTTYPEHANDLER

private:
    nsInstanceCounter            mInstanceCounter;
}; // nsUnknownContentTypeHandler

// HandleUnknownContentType (from nsIUnknownContentTypeHandler) implementation.
NS_IMETHODIMP
nsUnknownContentTypeHandler::HandleUnknownContentType( nsIChannel *aChannel,
                                                       const char *aContentType,
                                                       nsIDOMWindow *aWindow ) {
    if ( !aWindow ) {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_OK;

    // Open "Unknown content type" dialog.
    // We pass in the url and the content type.
    // Note that the "parent" browser window will be window.opener within the
    // new dialog.

#ifndef NECKO
    // nsIChannel is really an nsIURI, I guess.
    nsIChannel *channelUri = aChannel;

    // url string is const in this case.
    const char *urlStr = 0;
#else  // NECKO
    // Extract URI from channel.
    nsCOMPtr<nsIURI> channelUri = nsnull;
    rv = aChannel->GetURI(getter_AddRefs(channelUri));
    if ( NS_FAILED( rv ) ) {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: GetURI failed, rv=0x%08X\n",
                      (char*)__FILE__, (int)__LINE__, (int)rv );
        return rv;
    }

    // url string non-const in this case.
    char *urlStr = 0;
#endif // NECKO

    // Get url string from channel.
    channelUri->GetSpec( &urlStr );

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
                                                "svsss",
                                                "chrome://global/content/unknownContent.xul",
                                                JSVAL_NULL,
                                                "chrome",
                                                urlStr,
                                                aContentType );
#ifdef NECKO
                // Free url string.
                nsCRT::free(urlStr);
#endif // NECKO
                if ( argv ) {
                    nsIDOMWindow *newWindow;
                    rv = aWindow->OpenDialog( jsContext, argv, 5, &newWindow );
                    if ( NS_SUCCEEDED( rv ) ) {
                        newWindow->Release();
                    } else {
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

    return rv;
}

// Generate base nsIAppShellComponent implementation.
NS_IMPL_IAPPSHELLCOMPONENT( nsUnknownContentTypeHandler,
                            nsIUnknownContentTypeHandler,
                            NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID,
                            0 )
