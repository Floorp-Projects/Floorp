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
#include "nsISampleAppShellComponent.h"

#include "nsIAppShellComponentImpl.h"

#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsIObserver.h"

#if 0
#include "nsCOMPtr.h"
#include "pratom.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "prprf.h"

#include "nsIDocumentObserver.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsIStreamTransfer.h"
#endif

// You'll have to generate your own CID for your component...
// {CFC599F0-04CA-11d3-8068-00600811A9C3}
#define NS_SAMPLEAPPSHELLCOMPONENT_CID \
    { 0xcfc599f0, 0x4ca, 0x11d3, { 0x80, 0x68, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

// Implementation of the sample app shell component interface.
class nsSampleAppShellComponent : public nsISampleAppShellComponent,
                                  public nsAppShellComponentImpl {
public:
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_SAMPLEAPPSHELLCOMPONENT_CID );

    // ctor/dtor
    nsSampleAppShellComponent() {
        NS_INIT_REFCNT();
    }
    virtual ~nsSampleAppShellComponent() {
    }

    virtual PRBool IsService() {
        return PR_FALSE; // Don't register as a service.
    }

    NS_IMETHOD DoInitialization() {
        // Add your stuff here.
        return NS_OK;
    }

    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIAppShellComponent interface functions.
    NS_DECL_NSIAPPSHELLCOMPONENT

    // This class implements the nsISampleAppShellComponent interface functions.
    NS_DECL_NSISAMPLEAPPSHELLCOMPONENT

private:
    // Data members and implemention functions go here.

    // Objects of this class are counted to manage library unloading...
    nsInstanceCounter instanceCounter;
}; // nsSampleAppShellComponent


NS_IMETHODIMP
nsSampleAppShellComponent::DoDialogTests( nsISupports *parent, nsIObserver *observer ) {
    nsresult rv = NS_OK;
    DEBUG_PRINTF( PR_STDOUT, "nsSampleAppShellComponent::DoDialogTests called\n" );

    if ( parent && observer ) {
    // Open the dialog from C++.
    nsCOMPtr<nsIDOMWindowInternal> parentWindow = do_QueryInterface( parent, &rv );

    if ( NS_SUCCEEDED( rv ) ) {
        // Get JS context from parent window.
        nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( parentWindow, &rv );
        if ( NS_SUCCEEDED( rv ) ) {
            nsCOMPtr<nsIScriptContext> context;
            sgo->GetContext( getter_AddRefs( context ) );
            if ( context ) {
                JSContext *jsContext = (JSContext*)context->GetNativeContext();
                if ( jsContext ) {
                    // XXX Since xpconnect now has a JS argument converter for
                    // interfaces this explicit wrapping could be avoided.
                    //
                    // Convert observer to jsval so we can pass it as argument.
                    static NS_DEFINE_CID( kXPConnectCID, NS_XPCONNECT_CID );
                    NS_WITH_SERVICE( nsIXPConnect, xpc, kXPConnectCID, &rv );

                    if ( NS_SUCCEEDED( rv ) ) {
                        nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
                        rv = xpc->WrapNative( jsContext,
                                              JS_GetGlobalObject(jsContext),
                                              observer,
                                              NS_GET_IID(nsIObserver),
                                              getter_AddRefs( wrapper ) );
                        if ( NS_SUCCEEDED( rv ) ) {
                          JSObject* obj;
                          rv = wrapper->GetJSObject( &obj );
                          if ( NS_SUCCEEDED( rv ) ) {
                            // Get a jsval corresponding to the wrapped object.
                            jsval arg = OBJECT_TO_JSVAL( obj );
                            void *stackPtr;
                            jsval *argv = JS_PushArguments( jsContext, &stackPtr, "sssv",
                                                            "resource:/res/samples/nsSampleAppShellComponent.xul",
                                                            "foobar",
                                                            "chrome",
                                                            arg );
                            if ( argv ) {
                                nsIDOMWindowInternal *newWindow;
                                rv = parentWindow->OpenDialog( jsContext, argv, 4, &newWindow );
                                if ( NS_SUCCEEDED( rv ) ) {
                                    newWindow->Release();
                                } else {
                                }
                                JS_PopArguments( jsContext, stackPtr );
                            } else {
                            }
                          }
                        }
                        //? NS_RELEASE(aSupports);
                    } else {
                    }
                } else {
                }
            } else {
            }
        } else {
        }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: QueryInterface failed, rv=0x%08X\n",
                      __FILE__, (int)__LINE__, (int)rv );
    }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: DoDialogTests was passed a null pointer!\n",
                      __FILE__, (int)__LINE__ );
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

// Generate base nsIAppShellComponent implementation.
NS_IMPL_IAPPSHELLCOMPONENT( nsSampleAppShellComponent,
                            nsISampleAppShellComponent,
                            NS_ISAMPLEAPPSHELLCOMPONENT_PROGID,
                            0 )
