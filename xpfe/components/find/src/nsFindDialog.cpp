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

#include "nsIAppShellService.h"
#include "nsIFindComponent.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsITextServicesDocument.h"
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"

#include "nsFindComponent.h"
#include "nsFindDialog.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// Standard implementations of addref/release.
NS_IMPL_ADDREF( nsFindDialog );
NS_IMPL_RELEASE( nsFindDialog );

NS_IMETHODIMP 
nsFindDialog::QueryInterface( REFNSIID anIID, void **anInstancePtr)
{
    nsresult rv = NS_OK;

    if ( anInstancePtr ) {
        // Always NULL result, in case of failure
        *anInstancePtr = 0;

        // Check for interfaces we support and cast this appropriately.
        if ( anIID.Equals( nsIXULWindowCallbacks::GetIID() ) ) {
            *anInstancePtr = (void*) ((nsIXULWindowCallbacks*)this);
            NS_ADDREF_THIS();
        } else if ( anIID.Equals( nsIDocumentObserver::GetIID() ) ) {
            *anInstancePtr = (void*) ((nsIDocumentObserver*)this);
            NS_ADDREF_THIS();
        } else if ( anIID.Equals( kISupportsIID ) ) {
            *anInstancePtr = (void*) ((nsISupports*)(nsIDocumentObserver*)this);
            NS_ADDREF_THIS();
        } else {
            // Not an interface we support.
            rv = NS_ERROR_NO_INTERFACE;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

  return rv;
}

// ctor
nsFindDialog::nsFindDialog( nsIFindComponent         *aComponent,
                            nsFindComponent::Context *aContext )
        : mComponent( nsDontQueryInterface<nsIFindComponent>( aComponent ) ),
          mContext( aContext ),
          mWebShell(),
          mWindow() {
    // Initialize ref count.
    NS_INIT_REFCNT();
    mContext->AddRef();
}


// This is cribbed from nsBrowserAppCore.cpp also (and should be put somewhere once
// and reused)...
static int APP_DEBUG = 0;
static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value )
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIContentViewer> cv;
    rv = shell ? shell->GetContentViewer(getter_AddRefs(cv))
               : NS_ERROR_NULL_POINTER;
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( rv != NS_OK ) {
                             if (APP_DEBUG) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                        }
                    } else {
                        if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                  if (APP_DEBUG)   printf("Upcast to nsIDOMXULDocument failed\n");
                }
            } else {
                if (APP_DEBUG) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
             if (APP_DEBUG) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}


// Do startup stuff from C++ side.
NS_IMETHODIMP
nsFindDialog::ConstructBeforeJavaScript(nsIWebShell *aWebShell) {
    nsresult rv = NS_OK;

    // Save web shell pointer.
    mWebShell = nsDontQueryInterface<nsIWebShell>( aWebShell );

    // Store instance information into dialog's DOM.
    if ( mContext ) {
        setAttribute( mWebShell,
                      "data.searchString",
                      "value",
                      mContext->mSearchString );
        setAttribute( mWebShell,
                      "data.ignoreCase",
                      "value",
                      mContext->mIgnoreCase ? "true" : "false" );
        setAttribute( mWebShell,
                      "data.searchBackward",
                      "value",
                      mContext->mSearchBackwards ? "true" : "false" );
        setAttribute( mWebShell,
                      "data.wrap",
                      "value",
                      mContext->mWrapSearch ? "true" : "false" );
    }

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
                doc->AddObserver( this );
            } else {
            }
        } else {
        }
    } else {
    }

    return rv;
}

// Utility function to close a window given a root nsIWebShell.
static void closeWindow( nsIWebShellWindow *aWebShellWindow ) {
    if ( aWebShellWindow ) {
        // crashes!
        aWebShellWindow->Close();
    }
}

// Handle attribute changing; we only care about the element "data.execute"
// which is used to signal command execution from the UI.
NS_IMETHODIMP
nsFindDialog::AttributeChanged( nsIDocument *aDocument,
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
        aContent->GetAttribute( kNameSpaceID_None, atomCommand, cmd );
        // Reset command so we detect next request.
        aContent->SetAttribute( kNameSpaceID_None, atomCommand, "", PR_FALSE );
        if ( cmd == "find" ) {
            OnFind( aContent );
        } else if ( cmd == "next" ) {
            OnNext();
        } else if ( cmd == "cancel" ) {
            OnCancel();
        } else {
        }
    }

    return rv;
}

// OnOK
void
nsFindDialog::OnFind( nsIContent *aContent )
{
	if ( mWebShell && mContext )
	{
		nsAutoString		valueStr;

		// Get arguments and store into the search context.
		nsCOMPtr<nsIAtom> atomKey = nsDontQueryInterface<nsIAtom>( NS_NewAtom("key") );
		aContent->GetAttribute( kNameSpaceID_None, atomKey, mContext->mSearchString );

		nsCOMPtr<nsIAtom> atomIgnoreCase = nsDontQueryInterface<nsIAtom>( NS_NewAtom("ignoreCase") );
		aContent->GetAttribute( kNameSpaceID_None, atomIgnoreCase, valueStr );
		mContext->mIgnoreCase = (valueStr == "true");

		nsCOMPtr<nsIAtom> atomSearchBackwards = nsDontQueryInterface<nsIAtom>( NS_NewAtom("searchBackwards") );
		aContent->GetAttribute( kNameSpaceID_None, atomSearchBackwards, valueStr );
		mContext->mSearchBackwards = (valueStr == "true");

		nsCOMPtr<nsIAtom> atomWrapSearch = nsDontQueryInterface<nsIAtom>( NS_NewAtom("wrap") );
		aContent->GetAttribute( kNameSpaceID_None, atomWrapSearch, valueStr );
		mContext->mWrapSearch = (valueStr == "true");

		// Search for next occurrence.
		if ( mComponent )
		{
			// Find next occurrence in this context.
			mComponent->FindNext(mContext);
		}
	}
}

// OnOK
void
nsFindDialog::OnNext()
{
    if ( mContext && mComponent )
    {
        // Find next occurrence in this context.
        mComponent->FindNext(mContext);
    }
}

void
nsFindDialog::OnCancel()
{
    // Close the window.
    closeWindow( mWindow );
}

void
nsFindDialog::SetWindow( nsIWebShellWindow *aWindow )
{
    mWindow = nsDontQueryInterface<nsIWebShellWindow>(aWindow);
}

