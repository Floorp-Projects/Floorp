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
#include "nsISound.h"

#include "nsFindComponent.h"
#include "nsFindDialog.h"


// Standard implementations of addref/release.
NS_IMPL_ADDREF( nsFindDialog );
NS_IMPL_RELEASE( nsFindDialog );

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMETHODIMP 
nsFindDialog::QueryInterface( REFNSIID anIID, void **anInstancePtr)
{
    nsresult rv = NS_OK;

    if ( anInstancePtr ) {
        // Always NULL result, in case of failure
        *anInstancePtr = 0;

        // Check for interfaces we support and cast this appropriately.
        if ( anIID.Equals( ::nsIXULWindowCallbacks::GetIID() ) ) {
            *anInstancePtr = (void*) ((nsIXULWindowCallbacks*)this);
            NS_ADDREF_THIS();
        } else if ( anIID.Equals( ::nsIDocumentObserver::GetIID() ) ) {
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
        : mComponent( dont_QueryInterface(aComponent) ),
          mContext( aContext ),
          mDialogWebShell(),
          mDialogWindow(),
          mBeepIfNoFind(PR_TRUE)
{
    // Initialize ref count.
    NS_INIT_REFCNT();
    mContext->AddRef();
}


// This is cribbed from nsBrowserAppCore.cpp also (and should be put somewhere once
// and reused)...
static const int APP_DEBUG = 0;
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
nsFindDialog::ConstructBeforeJavaScript(nsIWebShell *aWebShell)
{
    nsresult rv = NS_OK;

    // Save web shell pointer.
    mDialogWebShell = dont_QueryInterface(aWebShell);

    // Store instance information into dialog's DOM.
    if ( mContext ) {
        setAttribute( mDialogWebShell,
                      "data.findKey",
                      "value",
                      mContext->mSearchString );
        setAttribute( mDialogWebShell,
                      "data.caseSensitive",
                      "value",
                      mContext->mCaseSensitive ? "true" : "false" );
        setAttribute( mDialogWebShell,
                      "data.searchBackward",
                      "value",
                      mContext->mSearchBackwards ? "true" : "false" );
        setAttribute( mDialogWebShell,
                      "data.wrap",
                      "value",
                      mContext->mWrapSearch ? "true" : "false" );
    }

    // Add as observer of the xul document.
    nsCOMPtr<nsIContentViewer> cv;
    rv = mDialogWebShell->GetContentViewer(getter_AddRefs(cv));
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

    // Trigger dialog startup.
    setAttribute( mDialogWebShell, "dialog.start", "ready", "true" );

    return rv;
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
    nsString idStr;
    nsCOMPtr<nsIAtom> atomId = dont_QueryInterface(NS_NewAtom("id"));
    aContent->GetAttribute( kNameSpaceID_None, atomId, idStr );
    if ( idStr == "data.execute" ) {
        nsString cmd;
        nsCOMPtr<nsIAtom> atomCommand = dont_QueryInterface(NS_NewAtom("command"));
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
	if ( mDialogWebShell && mContext )
	{
		nsAutoString		valueStr;

		// Get arguments and store into the search context.
		nsCOMPtr<nsIAtom> atomKey = dont_QueryInterface(NS_NewAtom("findKey"));
		aContent->GetAttribute( kNameSpaceID_None, atomKey, mContext->mSearchString );

		nsCOMPtr<nsIAtom> atomCaseSensitive = dont_QueryInterface(NS_NewAtom("caseSensitive"));
		aContent->GetAttribute( kNameSpaceID_None, atomCaseSensitive, valueStr );
		mContext->mCaseSensitive = (valueStr == "true");

		nsCOMPtr<nsIAtom> atomSearchBackwards = dont_QueryInterface(NS_NewAtom("searchBackwards"));
		aContent->GetAttribute( kNameSpaceID_None, atomSearchBackwards, valueStr );
		mContext->mSearchBackwards = (valueStr == "true");

		nsCOMPtr<nsIAtom> atomWrapSearch = dont_QueryInterface(NS_NewAtom("wrap"));
		aContent->GetAttribute( kNameSpaceID_None, atomWrapSearch, valueStr );
		mContext->mWrapSearch = (valueStr == "true");

		// Search for next occurrence.
		if ( mComponent )
		{
		  PRBool  foundMatch;
			// Find next occurrence in this context.
			mComponent->FindNext(mContext, &foundMatch);
      IndicateSuccess(foundMatch);
		}
	}
}

// OnOK
void
nsFindDialog::OnNext()
{
  if ( mContext && mComponent )
  {
	  PRBool  foundMatch;
      // Find next occurrence in this context.
    mComponent->FindNext(mContext, &foundMatch);
    IndicateSuccess(foundMatch);
  }
}

void
nsFindDialog::OnCancel()
{
    // Close the window.
    if (mDialogWindow)
    	mDialogWindow->Close();
}

void
nsFindDialog::SetWindow( nsIWebShellWindow *aWindow )
{
    mDialogWindow = nsDontQueryInterface<nsIWebShellWindow>(aWindow);
}

void
nsFindDialog::IndicateSuccess(PRBool aMatchFound)
{
  if (mBeepIfNoFind && !aMatchFound)
  {
    nsCOMPtr<nsISound> soundThang;
    nsComponentManager::CreateInstance("component://netscape/sound", nsnull, nsISound::GetIID(), getter_AddRefs(soundThang));
    if (soundThang)
      soundThang->Beep();
  }
}


