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

#include "nsIFindComponent.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAppShellService.h"
#include "nsIXULWindowCallbacks.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIURL.h"
#include "nsFileSpec.h"
#include "nsIFactory.h"
#include "pratom.h"
#include "nsIServiceManager.h"

static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value );

// {4AA267A0-F81D-11d2-8067-00600811A9C3}
#define NS_FINDCOMPONENT_CID \
    { 0x4aa267a0, 0xf81d, 0x11d2, { 0x80, 0x67, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3} }

struct nsFindComponent : public nsIFindComponent {
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_FINDCOMPONENT_CID );

    // ctor/dtor
    nsFindComponent();
    virtual ~nsFindComponent();

    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIAppShellComponent interface functions.
    NS_DECL_IAPPSHELLCOMPONENT

    // This class implements the nsIFindComponent interface functions.
    NS_DECL_IFINDCOMPONENT

    // "Context" for this implementation.
    struct Context : public nsISupports {
        NS_DECL_ISUPPORTS
        Context( nsIDocument *aDocument,
                 const nsString &lastSearchString,
                 const nsString &lastIgnoreCase,
                 const nsString &lastSearchBackward )
            : mDocument(),
              mSearchString( lastSearchString ),
              mIgnoreCase( lastIgnoreCase ),
              mSearchBackward( lastSearchBackward ) {
            // Construct nsITextServicesDocument...
            mDocument = nsDontQueryInterface<nsIDocument>( aDocument );

            NS_INIT_REFCNT();
        }
        virtual ~Context() {
        }
        void Reset( nsIDocument *aNewDocument ) {
            // Reconstruct nsITextServicesDocument?...
            mDocument = nsDontQueryInterface<nsIDocument>( aNewDocument );
        }
        // Maybe add Find/FindNext functions here?
        nsCOMPtr<nsIDocument> mDocument;
        nsString              mSearchString;
        nsString              mIgnoreCase;
        nsString              mSearchBackward;
    }; // nsFindComponent::Context

private:
    nsCOMPtr<nsIAppShellService> mAppShell;
    nsString                     mLastSearchString;
    nsString                     mLastIgnoreCase;
    nsString                     mLastSearchBackward;
}; // nsFindComponent

// ctor
nsFindComponent::nsFindComponent()
    : mAppShell(),
      mLastSearchString(),
      mLastIgnoreCase("false"),
      mLastSearchBackward("false") {
    NS_INIT_REFCNT();

    // Initialize "last" stuff from prefs, if we wanted to be really clever...
}

// dtor
nsFindComponent::~nsFindComponent() {
}

// nsISupports Implementation
NS_IMPL_ADDREF( nsFindComponent );
NS_IMPL_RELEASE( nsFindComponent );

NS_IMETHODIMP
nsFindComponent::QueryInterface( REFNSIID anIID, void **anInstancePtr ) {
    nsresult rv = NS_OK;

    // Check for place to return result.
    if ( !anInstancePtr ) {
        rv = NS_ERROR_NULL_POINTER;
    } else {
        // Initialize result.
        *anInstancePtr = 0;

        // Check for IIDs we support and cast this appropriately.
        if ( anIID.Equals( nsIFindComponent::GetIID() ) ) {
            *anInstancePtr = (void*) this;
            NS_ADDREF_THIS();
        } else if ( anIID.Equals( nsIAppShellComponent::GetIID() ) ) {
            *anInstancePtr = (void*) ( (nsIAppShellComponent*)this );
            NS_ADDREF_THIS();
        } else if ( anIID.Equals( nsISupports::GetIID() ) ) {
            *anInstancePtr = (void*) ( (nsISupports*)this );
            NS_ADDREF_THIS();
        } else {
            // Not an interface we support.
            rv = NS_NOINTERFACE;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsFindComponent::Initialize( nsIAppShellService *appShell,
                             nsICmdLineService *args ) {
    nsresult rv = NS_OK;

    // Remember the app shell service in case we need it.
    mAppShell = nsDontQueryInterface<nsIAppShellService>( appShell );

    return rv;
}

NS_IMETHODIMP
nsFindComponent::CreateContext( nsIDocument *aDocument,
                                nsISupports **aResult ) {
    nsresult rv = NS_OK;

    // Check for place to put result.
    if ( !aResult ) {
        rv = NS_ERROR_NULL_POINTER;
    } else {
        // Construct a new Context with this document.
        *aResult = new Context( aDocument,
                                mLastSearchString,
                                mLastIgnoreCase,
                                mLastSearchBackward );
        if ( *aResult ) {
            // Do the expected on behalf of caller.
            (*aResult)->AddRef();
        } else {
            // Allocation failed.
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return rv;
}

// Cribbed from nsFileDownloadDialog in nsBrowsrAppCore.cpp.  I really must
// figure out how to make this more reusable...
struct nsFindDialog : public nsIXULWindowCallbacks,
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

    // nsFindDialog stuff
    nsFindDialog( nsIFindComponent *aComponent,
                  nsFindComponent::Context *aContext );
    virtual ~nsFindDialog() { NS_IF_RELEASE(mContext); }
    void OnFind( nsIContent *aContent );
    void OnNext();
    void OnCancel();
    void SetWindow( nsIWebShellWindow *aWindow );

private:
    nsCOMPtr<nsIFindComponent>         mComponent;
    nsFindComponent::Context          *mContext;
    nsCOMPtr<nsIWebShell>              mWebShell;
    nsCOMPtr<nsIWebShellWindow>        mWindow;
}; // nsFindDialog

// Standard implementations of addref/release.
NS_IMPL_ADDREF( nsFindDialog );
NS_IMPL_RELEASE( nsFindDialog );
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMETHODIMP 
nsFindDialog::QueryInterface( REFNSIID anIID, void **anInstancePtr) {
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
                      mContext->mIgnoreCase.GetUnicode() ? "true" : "false" );
        setAttribute( mWebShell,
                      "data.searchBackward",
                      "value",
                      mContext->mSearchBackward.GetUnicode() ? "true" : "false" );
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
        if ( cmd == "find" ) {
            OnFind( aContent );
        } else if ( cmd == "next" ) {
            OnNext();
        } else if ( cmd == "cancel" ) {
            OnCancel();
        } else {
        }
        // Reset command so we detect next request.
        aContent->SetAttribute( kNameSpaceID_None, atomCommand, "", PR_FALSE );
    }

    return rv;
}

// OnOK
void
nsFindDialog::OnFind( nsIContent *aContent ) {
    if ( mWebShell && mContext ) {
        // Get arguments and store into the search context.
        nsCOMPtr<nsIAtom> atomKey = nsDontQueryInterface<nsIAtom>( NS_NewAtom("key") );
        aContent->GetAttribute( kNameSpaceID_None, atomKey, mContext->mSearchString );
        nsCOMPtr<nsIAtom> atomIgnoreCase = nsDontQueryInterface<nsIAtom>( NS_NewAtom("ignoreCase") );
        aContent->GetAttribute( kNameSpaceID_None, atomIgnoreCase, mContext->mIgnoreCase );
        nsCOMPtr<nsIAtom> atomSearchBackward = nsDontQueryInterface<nsIAtom>( NS_NewAtom("searchBackward") );
        aContent->GetAttribute( kNameSpaceID_None, atomSearchBackward, mContext->mSearchBackward );

        // Search for next occurrence.
        OnNext();
    }
}

// OnOK
void
nsFindDialog::OnNext() {
    if ( mContext && mComponent ) {
        // Find next occurrence in this context.
        mComponent->FindNext( mContext );
    }
}

void
nsFindDialog::OnCancel() {
    // Close the window.
    closeWindow( mWindow );
}

void
nsFindDialog::SetWindow( nsIWebShellWindow *aWindow ) {
    mWindow = nsDontQueryInterface<nsIWebShellWindow>(aWindow);
}

NS_IMETHODIMP
nsFindComponent::Find( nsISupports *aContext ) {
    nsresult rv = NS_OK;

    if ( aContext && mAppShell ) {
        Context *context = (Context*)aContext;

        // Open Find dialog and prompt for search parameters.
        nsString controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
        nsIWebShellWindow *newWindow;

        // Make url for dialog xul.
        nsIURL *url;
        rv = NS_NewURL( &url, "resource:/res/samples/finddialog.xul" );

        // Create callbacks object for the find dialog.
        nsFindDialog *dialog = new nsFindDialog( this, context );

        rv = mAppShell->CreateTopLevelWindow( nsnull,
                                              url,
                                              controllerCID,
                                              newWindow,
                                              nsnull,
                                              dialog,
                                              425,
                                              200 );

        if ( NS_SUCCEEDED( rv ) ) {
            // Tell the dialog its nsIWebShellWindow.
            dialog->SetWindow( newWindow );
        }

        // Release the url for the xul file.
        NS_RELEASE( url );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsFindComponent::FindNext( nsISupports *aContext ) {
    nsresult rv = NS_OK;

    if ( aContext ) {
        // For now, just record request to console.
        Context *context = (Context*)aContext;
        printf( "nsFindComponent::FindNext\n\tkey=%s\n\tignoreCase=%s\tsearchBackward=%s\n",
                (const char *)nsAutoCString( context->mSearchString ),
                (const char *)nsAutoCString( context->mIgnoreCase ),
                (const char *)nsAutoCString( context->mSearchBackward ) );

        // Record this for out-of-the-blue FindNext calls.
        mLastSearchString   = context->mSearchString;
        mLastIgnoreCase     = context->mIgnoreCase;
        mLastSearchBackward = context->mSearchBackward;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsFindComponent::ResetContext( nsISupports *aContext,
                               nsIDocument *aNewDocument ) {
    nsresult rv = NS_OK;
    if ( aContext && aNewDocument ) {
        // Pass on the new document to the context.
        Context *context = (Context*)aContext;
        context->Reset( aNewDocument );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

// nsFindComponent::Context implementation...
NS_IMPL_ISUPPORTS( nsFindComponent::Context, nsISupports::GetIID() )

// This is cribbed from nsBrowserAppCore.cpp also (and should be put somewhere once
// and reused)...
static int APP_DEBUG = 0;
static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
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


static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

// Factory stuff
struct nsFindComponentFactory : public nsIFactory {   
    // ctor/dtor
    nsFindComponentFactory() {
        NS_INIT_REFCNT();
    }
    virtual ~nsFindComponentFactory() {
    }

    // This class implements the nsISupports interface functions.
	NS_DECL_ISUPPORTS 

    // nsIFactory methods   
    NS_IMETHOD CreateInstance( nsISupports *aOuter,
                               const nsIID &aIID,
                               void **aResult );   
    NS_IMETHOD LockFactory( PRBool aLock );
};   

// nsISupports interface implementation.
NS_IMPL_ADDREF(nsFindComponentFactory)
NS_IMPL_RELEASE(nsFindComponentFactory)
NS_IMETHODIMP
nsFindComponentFactory::QueryInterface( const nsIID &anIID, void **aResult ) {   
    nsresult rv = NS_OK;

    if ( aResult ) {
        *aResult = 0;
        if ( 0 ) {
        } else if ( anIID.Equals( nsIFactory::GetIID() ) ) {
            *aResult = (void*) (nsIFactory*)this;
            NS_ADDREF_THIS();
        } else if ( anIID.Equals( nsISupports::GetIID() ) ) {
            *aResult = (void*) (nsISupports*)this;
            NS_ADDREF_THIS();
        } else {
            rv = NS_ERROR_NO_INTERFACE;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}   

NS_IMETHODIMP
nsFindComponentFactory::CreateInstance( nsISupports *anOuter,
                                        const nsIID &anIID,
                                        void*       *aResult ) {
    nsresult rv = NS_OK;

    if ( aResult ) {
        // Allocate new find component object.
        nsFindComponent *component = new nsFindComponent();

        if ( component ) {
            // Allocated OK, do query interface to get proper
            // pointer and increment refcount.
            rv = component->QueryInterface( anIID, aResult );
            if ( NS_FAILED( rv ) ) {
                // refcount still at zero, delete it here.
                delete component;
            }
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}  

NS_IMETHODIMP
nsFindComponentFactory::LockFactory(PRBool aLock) {  
	if (aLock)
		PR_AtomicIncrement(&g_LockCount); 
	else
		PR_AtomicDecrement(&g_LockCount);

	return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf( nsISupports* aServiceMgr, const char* path ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIServiceManager> serviceMgr( do_QueryInterface( aServiceMgr, &rv ) );

    if ( NS_SUCCEEDED( rv ) ) {
        // Get the component manager service.
        nsCID cid = NS_COMPONENTMANAGER_CID;
        nsIComponentManager *componentMgr = 0;
        rv = serviceMgr->GetService( cid,
                                     nsIComponentManager::GetIID(),
                                     (nsISupports**)&componentMgr );
        if ( NS_SUCCEEDED( rv ) ) {
            // Register our component.
            rv = componentMgr->RegisterComponent( nsFindComponent::GetCID(),
                                                  NS_IFINDCOMPONENT_CLASSNAME,
                                                  NS_IFINDCOMPONENT_PROGID,
                                                  path,
                                                  PR_TRUE,
                                                  PR_TRUE );

            #ifdef NS_DEBUG
            if ( NS_SUCCEEDED( rv ) ) {
                printf( "nsFindComponent's NSRegisterSelf successful\n" );
            } else {
                printf( "nsFindComponent's NSRegisterSelf failed, RegisterComponent rv=0x%X\n", (int)rv );
            }
            #endif

            // Release the component manager service.
            serviceMgr->ReleaseService( cid, componentMgr );
        } else {
            #ifdef NS_DEBUG
            printf( "nsFindComponent's NSRegisterSelf failed, GetService rv=0x%X\n", (int)rv );
            #endif
        }
    } else {
        #ifdef NS_DEBUG
        printf( "nsFindComponent's NSRegisterSelf failed, QueryInterface rv=0x%X\n", (int)rv );
        #endif
    }

    return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf( nsISupports* aServiceMgr, const char* path ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIServiceManager> serviceMgr( do_QueryInterface( aServiceMgr, &rv ) );

    if ( NS_SUCCEEDED( rv ) ) {
        // Get the component manager service.
        nsCID cid = NS_COMPONENTMANAGER_CID;
        nsIComponentManager *componentMgr = 0;
        rv = serviceMgr->GetService( cid,
                                     nsIComponentManager::GetIID(),
                                     (nsISupports**)&componentMgr );
        if ( NS_SUCCEEDED( rv ) ) {
            // Register our component.
            rv = componentMgr->UnregisterComponent( nsFindComponent::GetCID(), path );

            #ifdef NS_DEBUG
            if ( NS_SUCCEEDED( rv ) ) {
                printf( "nsFindComponent's NSUnregisterSelf successful\n" );
            } else {
                printf( "nsFindComponent's NSUnregisterSelf failed, UnregisterComponent rv=0x%X\n", (int)rv );
            }
            #endif

            // Release the component manager service.
            serviceMgr->ReleaseService( cid, componentMgr );
        } else {
            #ifdef NS_DEBUG
            printf( "nsFindComponent's NSRegisterSelf failed, GetService rv=0x%X\n", (int)rv );
            #endif
        }
    } else {
        #ifdef NS_DEBUG
        printf( "nsFindComponent's NSRegisterSelf failed, QueryInterface rv=0x%X\n", (int)rv );
        #endif
    }

    return rv;
}

extern "C" NS_EXPORT nsresult
NSGetFactory( nsISupports *aServMgr,
              const nsCID &aClass,
              const char  *aClassName,
              const char  *aProgID,
              nsIFactory* *aFactory ) {
    nsresult rv = NS_OK;

    if ( aFactory ) {
        nsFindComponentFactory *factory = new nsFindComponentFactory();
        if ( factory ) {
            rv = factory->QueryInterface( nsIFactory::GetIID(), (void**)aFactory );
            if ( NS_FAILED( rv ) ) {
                #ifdef NS_DEBUG
                printf( "nsFindComponent's NSGetFactory failed, QueryInterface rv=0x%X\n", (int)rv );
                #endif
                // Delete this bogus factory.
                delete factory;
            }
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

extern "C" NS_EXPORT PRBool
NSCanUnload( nsISupports* aServiceMgr ) {
    PRBool result = g_InstanceCount == 0 && g_LockCount == 0;
    return result;
}
