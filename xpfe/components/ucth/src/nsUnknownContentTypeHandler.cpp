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
#include "nsIUnknownContentTypeHandler.h"

#include "nsIAppShellComponentImpl.h"

#include "nsIXULWindowCallbacks.h"
#include "nsIDocumentObserver.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsIWebShellWindow.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIWebShell.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"

#include "nsIStreamTransfer.h"

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

struct nsUnknownContentDialog : public nsIXULWindowCallbacks,
                                       nsIDocumentObserver {
    // Declare implementation of ISupports stuff.
    NS_DECL_ISUPPORTS

    // Declare implementations of nsIXULWindowCallbacks interface functions.
    NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
    NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

    // Declare implementations of nsIDocumentObserver functions.
    NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
    NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
    NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                              nsIContent* aContent,
                              nsISupports* aSubContent) { return NS_OK; }
    NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                    nsIContent* aContent1,
                                    nsIContent* aContent2) { return NS_OK; }
    // This one we care about; see implementation below.
    NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                                nsIContent*  aContent,
                                nsIAtom*     aAttribute,
                                PRInt32      aHint);
    NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               PRInt32     aNewIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
    NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                                 nsIStyleSheet* aStyleSheet) { return NS_OK; }
    NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                              nsIStyleSheet* aStyleSheet,
                                              PRBool aDisabled) { return NS_OK; }
    NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule,
                                PRInt32 aHint) { return NS_OK; }
    NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
    NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule) { return NS_OK; }
    NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { return NS_OK; }

    // nsUnknownContentDialog stuff
    nsUnknownContentDialog( nsIURL *aURL, const char *aContentType )
        : mUrl( aURL ),
          mContentType( aContentType ) {
        NS_INIT_REFCNT();
    }
    ~nsUnknownContentDialog() {
    }
    void SetWindow( nsIWebShellWindow *aWindow ) {
        mWindow = aWindow;
    }
    void OnSave();
    void OnMore() {
        // Not yet implemented.
    }
    void OnPick() {
        // Not yet implemented.
    }
    void OnClose() {
        if ( mWindow ) {
            mWindow->Close();
        }
    }

private:
    nsCOMPtr<nsIURL>            mUrl;
    nsCOMPtr<nsIWebShell>       mWebShell;
    nsCOMPtr<nsIWebShellWindow> mWindow;
    nsString                    mContentType;
    nsInstanceCounter           mInstanceCounter;
}; // nsUnknownContentDialog

// Standard implementations of addref/release.
NS_IMPL_ADDREF( nsUnknownContentDialog );
NS_IMPL_RELEASE( nsUnknownContentDialog );

// QueryInterface implementation for nsUnknownContentTypeDialog.
NS_IMETHODIMP 
nsUnknownContentDialog::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(nsIDocumentObserver::GetIID())) {
    *aInstancePtr = (void*) ((nsIDocumentObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIXULWindowCallbacks::GetIID())) {
    *aInstancePtr = (void*) ((nsIXULWindowCallbacks*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

// HandleUnknownContentType (from nsIUnknownContentTypeHandler) implementation.
NS_IMETHODIMP
nsUnknownContentTypeHandler::HandleUnknownContentType( nsIURL *aURL,
                                                       const char *aContentType,
                                                       nsIDocumentLoader *aDocLoader ) {
    nsresult rv = NS_OK;

    // Make sure we've been initialized.
    if ( mAppShell ) {
        // Open "Unknown file type" dialog.
        nsIWebShellWindow *newWindow;
    
        // Make url for dialog xul.
        nsIURL *url;
        rv = NS_NewURL( &url, "resource:/res/samples/unknownContent.xul" );
    
        if ( NS_SUCCEEDED(rv) ) {
            // Create "save to disk" nsIXULCallbacks...
            nsUnknownContentDialog *dialog = new nsUnknownContentDialog( aURL, aContentType );
    
            rv = mAppShell->CreateTopLevelWindow( nsnull,
                                                  url,
                                                  PR_TRUE,
                                                  newWindow,
                                                  nsnull,
                                                  dialog,
                                                  0, 0 );
    
            // Give find dialog the window pointer (if it worked).
            if ( NS_SUCCEEDED(rv) ) {
                dialog->SetWindow( newWindow );
            }
    
            NS_RELEASE(url);
        }
    } else {
        rv = NS_ERROR_NOT_INITIALIZED;
    }

    return rv;
}

#ifdef NS_DEBUG
    #define DEBUG_PRINTF PR_fprintf
#else
    #define DEBUG_PRINTF (void)
#endif

// Do startup stuff from C++ side.
NS_IMETHODIMP
nsUnknownContentDialog::ConstructBeforeJavaScript( nsIWebShell *aWebShell ) {
    nsresult rv = NS_OK;

    // Save web shell pointer.
    mWebShell = aWebShell;

    // Add as observer of the xul document.
    nsCOMPtr<nsIContentViewer> cv;
    rv = mWebShell->GetContentViewer(getter_AddRefs(cv));
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Add this as observer.
                doc->AddObserver( this );

                // Store instance information into dialog's DOM.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) ); // up cast
                if ( xulDoc ) {
                    // Set data.location value attribute to the url.
                    nsCOMPtr<nsIDOMElement> location;
                    rv = xulDoc->GetElementById( "data.location", getter_AddRefs(location) );
                    if ( location ) {
                        const char *loc = 0;
                        mUrl->GetSpec( &loc );
                        rv = location->SetAttribute( "value", loc );
                        if ( NS_SUCCEEDED( rv ) ) {
                            // Set data.contentType value attribute to the content type.
                            nsCOMPtr<nsIDOMElement> contentType;
                            rv = xulDoc->GetElementById( "data.contentType", getter_AddRefs(contentType) );
                            if ( contentType ) {
                                rv = contentType->SetAttribute( "value", mContentType );
                                if ( NS_SUCCEEDED( rv ) ) {
                                    // Set dialog.start value attribute to trigger onLoad().
                                    nsCOMPtr<nsIDOMElement> trigger;
                                    rv = xulDoc->GetElementById( "dialog.start", getter_AddRefs(trigger) );
                                    if ( trigger ) {
                                        rv = trigger->SetAttribute( "ready", "true" );
                                        if ( NS_SUCCEEDED( rv ) ) {
                                        } else {
                                            DEBUG_PRINTF( PR_STDOUT, "SetAttribute failed, rv=0x%X\n", (int)rv );
                                        }
                                    } else {
                                        DEBUG_PRINTF( PR_STDOUT, "GetElementById failed, rv=0x%X\n", (int)rv );
                                    }
                                } else {
                                    DEBUG_PRINTF( PR_STDOUT, "SetAttribute failed, rv=0x%X\n", (int)rv );
                                }
                            } else {
                                DEBUG_PRINTF( PR_STDOUT, "GetElementById failed, rv=0x%X\n", (int)rv );
                            }
                        } else {
                            DEBUG_PRINTF( PR_STDOUT, "SetAttribute failed, rv=0x%X\n", (int)rv );
                        }
                    } else {
                        DEBUG_PRINTF( PR_STDOUT, "GetElementById failed, rv=0x%X\n", (int)rv );
                    }
                } else {
                    DEBUG_PRINTF( PR_STDOUT, "Upcast to nsIDOMXULDocument failed\n" );
                }
            } else {
                DEBUG_PRINTF( PR_STDOUT, "GetDocument failed, rv=0x%X\n", (int)rv);
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "Upcast to nsIDocumentViewer failed\n" );
        }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "GetContentViewer failed, rv=0x%X\n", (int)rv);
    }

    return rv;
}
// Handle attribute changing; we only care about the element "data.execute"
// which is used to signal command execution from the UI.
NS_IMETHODIMP
nsUnknownContentDialog::AttributeChanged( nsIDocument *aDocument,
                                          nsIContent*  aContent,
                                          nsIAtom*     aAttribute,
                                          PRInt32      aHint ) {
    nsresult rv = NS_OK;
    // Look for data.execute command changing.
    nsString id;
    nsCOMPtr<nsIAtom> atomId = nsDontQueryInterface<nsIAtom>( NS_NewAtom("id") );
    aContent->GetAttribute( kNameSpaceID_None, atomId, id );
    if ( id == "data.execute" ) {
        nsString cmd;
        nsCOMPtr<nsIAtom> atomCommand = nsDontQueryInterface<nsIAtom>( NS_NewAtom("command") );
        // Get requested command.
        aContent->GetAttribute( kNameSpaceID_None, atomCommand, cmd );
        // Reset (immediately, to prevent feedback loop).
        aContent->SetAttribute( kNameSpaceID_None, atomCommand, "", PR_FALSE );
        if ( cmd == "save" ) {
            OnSave();
        } else if ( cmd == "more" ) {
            OnMore();
        } else if ( cmd == "pick" ) {
            OnPick();
        } else if ( cmd == "close" ) {
            OnClose();
        } else {
        }
    }

    return rv;
}

static NS_DEFINE_IID( kAppShellServiceCID, NS_APPSHELL_SERVICE_CID );

// OnSave: Pass on the URL to the "stream xfer" component.
void
nsUnknownContentDialog::OnSave() {
    nsresult rv = NS_OK;

    // Get "stream xfer component".
    nsIStreamTransfer *xfer;
    rv = nsAppShellComponentImpl::mServiceMgr->GetService( NS_ISTREAMTRANSFER_PROGID,
                                                           nsIStreamTransfer::GetIID(),
                                                           (nsISupports**)&xfer );

    if ( NS_SUCCEEDED( rv ) ) {
        // Have the component stream the url to a user-selected file.
        rv = xfer->SelectFileAndTransferLocation( mUrl );

        if ( NS_SUCCEEDED( rv ) ) {
            // Close this dialog.
            mWindow->Close();
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Error saving file, rv=0x%X\n",
                          __FILE__, (int)__LINE__, (int)rv );
        }
        nsAppShellComponentImpl::mServiceMgr->ReleaseService( NS_ISTREAMTRANSFER_PROGID, xfer );
    } else {
        DEBUG_PRINTF( PR_STDOUT, "Unable to get stream transfer, GetService rv=0x%X\n", (int)rv );
    }

    return;
}

// Generate base nsIAppShellComponent implementation.
NS_IMPL_IAPPSHELLCOMPONENT( nsUnknownContentTypeHandler,
                            nsIUnknownContentTypeHandler,
                            NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID )
