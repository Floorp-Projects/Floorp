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
#include "nsIHelperAppLauncherDialog.h"

#include "nsIAppShellComponentImpl.h"

#include "nsString.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIStreamObserver.h"
#include "nsIHTTPChannel.h"
#include "nsXPIDLString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIExternalHelperAppService.h"
#include "nsIStringBundle.h"
#include "nsIFilePicker.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
#define HELPERAPP_DIALOG_URL       "chrome://global/locale/helperAppLauncher.properties"

// {42770B50-03E9-11d3-8068-00600811A9C3}
#define NS_UNKNOWNCONTENTTYPEHANDLER_CID \
    { 0x42770b50, 0x3e9, 0x11d3, { 0x80, 0x68, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

class nsUnknownContentTypeHandler : public nsIUnknownContentTypeHandler,
                                    public nsIHelperAppLauncherDialog,
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

    // This class implements the nsIHelperAppLauncherDialog interface functions.
    NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

private:
    nsInstanceCounter            mInstanceCounter;

    // Module stuff.
    static NS_METHOD CreateComponent( nsISupports  *aOuter,
                                      REFNSIID      aIID,
                                      void        **aResult );
    static nsUnknownContentTypeHandler *mInstance;

public:
    static nsModuleComponentInfo components[];
}; // nsUnknownContentTypeHandler

// HandleUnknownContentType (from nsIUnknownContentTypeHandler) implementation.
// XXX We can get the content type from the channel now so that arg could be dropped.
NS_IMETHODIMP
nsUnknownContentTypeHandler::HandleUnknownContentType( nsIChannel *aChannel,
                                                       const char *aContentType,
                                                       nsIDOMWindowInternal *aWindow ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsISupports> channel;
    nsCAutoString         contentDisp;

    if ( aChannel ) {
        // Need root nsISupports for later JS_PushArguments call.
        channel = do_QueryInterface( aChannel );

        // Try to get HTTP channel.
        nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface( aChannel );
        if ( httpChannel ) {
            // Get content-disposition response header.
            nsCOMPtr<nsIAtom> atom = NS_NewAtom( "content-disposition" );
            if ( atom ) {
                nsXPIDLCString disp; 
                rv = httpChannel->GetResponseHeader( atom, getter_Copies( disp ) );
                if ( NS_SUCCEEDED( rv ) && disp ) {
                    contentDisp = disp; // Save the response header to pass to dialog.
                }
            }
        }

        // Cancel input channel now.
        rv = aChannel->Cancel(NS_BINDING_ABORTED);
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Cancel failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    if ( NS_SUCCEEDED( rv ) && channel && aContentType && aWindow ) {
        // Open "Unknown content type" dialog.
        // We pass in the channel, the content type, and the content disposition.
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
                                                    "sss%ipss",
                                                    "chrome://global/content/unknownContent.xul",
                                                    "_blank",
                                                    "chrome,titlebar",
                                                    (const nsIID*)(&NS_GET_IID(nsIChannel)),
                                                    (nsISupports*)channel.get(),
                                                    aContentType,
                                                    (const char*)contentDisp.GetBuffer() );
                    if ( argv ) {
                        nsCOMPtr<nsIDOMWindowInternal> newWindow;
                        rv = aWindow->OpenDialog( jsContext, argv, 6, getter_AddRefs( newWindow ) );
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

// Show the helper app launch confirmation dialog as instructed.
NS_IMETHODIMP
nsUnknownContentTypeHandler::Show( nsIHelperAppLauncher *aLauncher, nsISupports *aContext ) {
    nsresult rv = NS_ERROR_FAILURE;

    // Get parent window (from context).
    nsCOMPtr<nsIDOMWindowInternal> parent( do_GetInterface( aContext ) );
    if ( parent ) {
        // Get JS context from parent window.
        nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( parent, &rv );
        if ( NS_SUCCEEDED( rv ) && sgo ) {
            nsCOMPtr<nsIScriptContext> context;
            sgo->GetContext( getter_AddRefs( context ) );
            if ( context ) {
                // Get native context.
                JSContext *jsContext = (JSContext*)context->GetNativeContext();
                if ( jsContext ) {
                    // Set up window.arguments[0]...
                    void *stackPtr;
                    jsval *argv = JS_PushArguments( jsContext,
                                                    &stackPtr,
                                                    "sss%ip",
                                                    "chrome://global/content/helperAppLauncher.xul",
                                                    "_blank",
                                                    "chrome",
                                                    (const nsIID*)(&NS_GET_IID(nsIHelperAppLauncher)),
                                                    (nsISupports*)aLauncher );
                    if ( argv ) {
                        // Open the dialog.
                        nsCOMPtr<nsIDOMWindowInternal> dialog;
                        rv = parent->OpenDialog( jsContext, argv, 6, getter_AddRefs( dialog ) );
                        // Pop arguments.
                        JS_PopArguments( jsContext, stackPtr );

                    }
                }
            }
        }
    }
    return rv;
}

// prompt the user for a file name to save the unknown content to as instructed
NS_IMETHODIMP
nsUnknownContentTypeHandler::PromptForSaveToFile(nsISupports * aWindowContext, const PRUnichar * aDefaultFile, const PRUnichar * aSuggestedFileExtension, nsILocalFile ** aNewFile)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("component://mozilla/filepicker", &rv);
  if (filePicker)
  {
    nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
    nsCOMPtr<nsIStringBundle> stringBundle;
    NS_ENSURE_TRUE(stringService, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(stringService->CreateBundle(HELPERAPP_DIALOG_URL, nsnull, getter_AddRefs(stringBundle)), 
                    NS_ERROR_FAILURE);

    nsXPIDLString windowTitle;
    stringBundle->GetStringFromName(NS_LITERAL_STRING("saveDialogTitle"), getter_Copies(windowTitle));

    nsCOMPtr<nsIDOMWindowInternal> parent( do_GetInterface( aWindowContext ) );
    filePicker->Init(parent, windowTitle, nsIFilePicker::modeSave);
    filePicker->SetDefaultString(aDefaultFile);
    filePicker->AppendFilter(aSuggestedFileExtension, aSuggestedFileExtension);
    filePicker->AppendFilters(nsIFilePicker::filterAll);

    PRInt16 dialogResult;
    filePicker->Show(&dialogResult);
    if (dialogResult == nsIFilePicker::returnCancel)
      rv = NS_ERROR_FAILURE;
    else          
      rv = filePicker->GetFile(aNewFile);
  }

  return rv;
}


NS_IMETHODIMP      
nsUnknownContentTypeHandler::CreateComponent( nsISupports  *aOuter,
                                              REFNSIID      aIID,
                                              void        **aResult ) {                                                                  
    if ( !aResult ) {                                                
        return NS_ERROR_INVALID_POINTER;                           
    }                                                              

    if ( aOuter ) {                                                  
        *aResult = nsnull;                                         
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    
    if (mInstance == nsnull) {
        mInstance = new nsUnknownContentTypeHandler();
    }

    if ( mInstance == nsnull )
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = mInstance->QueryInterface( aIID, aResult );                        
    if ( NS_FAILED(rv) )  {                                             
        *aResult = nsnull;                                           
    }                                                                

    return rv;                                                       
}

nsUnknownContentTypeHandler* nsUnknownContentTypeHandler::mInstance = nsnull;

nsModuleComponentInfo nsUnknownContentTypeHandler::components[] = {
  { NS_IUNKNOWNCONTENTTYPEHANDLER_CLASSNAME, 
    NS_UNKNOWNCONTENTTYPEHANDLER_CID, 
    NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID, 
    nsUnknownContentTypeHandler::CreateComponent },
  { NS_IHELPERAPPLAUNCHERDLG_CLASSNAME, 
    NS_UNKNOWNCONTENTTYPEHANDLER_CID, 
    NS_IHELPERAPPLAUNCHERDLG_PROGID, 
    nsUnknownContentTypeHandler::CreateComponent },
};

NS_IMPL_NSGETMODULE( "nsUnknownContentTypeHandler", nsUnknownContentTypeHandler::components )

#if 0
// Generate base nsIAppShellComponent implementation.
NS_IMPL_IAPPSHELLCOMPONENT( nsUnknownContentTypeHandler,
                            nsIUnknownContentTypeHandler,
                            NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID,
                            0 )
#else

// XXX Cut/paste subset of nsIAppShellComponentImpl.h (just what we need).

// These make the macro source compile appropriately.
#define className     nsUnknownContentTypeHandler
#define interfaceName nsIUnknownContentTypeHandler
#define progId        NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID

/* Define instance counter implementation stuff. */
NS_DEFINE_MODULE_INSTANCE_COUNTER()
/* Define component globals. */
NS_DEFINE_COMPONENT_GLOBALS()
/* Component's implementation of Initialize. */
NS_IMETHODIMP 
className::Initialize( nsIAppShellService *anAppShell, 
                       nsICmdLineService  *aCmdLineService ) { 
    nsresult rv = NS_OK; 
    mAppShell = anAppShell; 
    mCmdLine  = aCmdLineService; 
    if ( Is_Service() ) { 
        rv = nsServiceManager::RegisterService( progId, (interfaceName*)this ); 
    } 
    if ( NS_SUCCEEDED( rv ) ) { 
        rv = DoInitialization(); 
    } 
    return rv; 
} 
/* Component's implementation of Shutdown. */
NS_IMETHODIMP 
className::Shutdown() { 
    nsresult rv = NS_OK; 
    if ( Is_Service() ) { 
        rv = nsServiceManager::UnregisterService( progId ); 
    } 
    return rv; 
} 
/* nsISupports Implementation for the class */
NS_IMPL_ADDREF( className );  
NS_IMPL_RELEASE( className ); 
/* QueryInterface implementation for this class. */
NS_IMETHODIMP 
className::QueryInterface( REFNSIID anIID, void **anInstancePtr ) { 
    nsresult rv = NS_OK; 
    /* Check for place to return result. */
    if ( !anInstancePtr ) { 
        rv = NS_ERROR_NULL_POINTER; 
    } else { 
        /* Initialize result. */
        *anInstancePtr = 0; 
        /* Check for IIDs we support and cast this appropriately. */
        if ( 0 ) { 
        } else if ( anIID.Equals( NS_GET_IID(interfaceName) ) ) { 
            *anInstancePtr = (void*) this; 
            NS_ADDREF_THIS(); 
        } else if ( anIID.Equals( NS_GET_IID(nsIHelperAppLauncherDialog) ) ) { 
            *anInstancePtr = (void*) (nsIHelperAppLauncherDialog*)this; 
            NS_ADDREF_THIS(); 
        } else if ( anIID.Equals( NS_GET_IID(nsIAppShellComponent) ) ) { 
            *anInstancePtr = (void*) ( (nsIAppShellComponent*)this ); 
            NS_ADDREF_THIS(); 
        } else if ( anIID.Equals( NS_GET_IID(nsISupports) ) ) { 
            *anInstancePtr = (void*) ( (nsISupports*) (interfaceName*)this ); 
            NS_ADDREF_THIS(); 
        } else { 
            /* Not an interface we support. */
            rv = NS_NOINTERFACE; 
        } 
    } 
    return rv; 
} 
#if 0
/* Factory class */
struct className##Factory : public nsIFactory { 
    /* ctor/dtor */
    className##Factory() { 
        NS_INIT_REFCNT(); 
    } 
    virtual ~className##Factory() { 
    } 
    /* This class implements the nsISupports interface functions. */
	NS_DECL_ISUPPORTS 
    /* nsIFactory methods */
    NS_IMETHOD CreateInstance( nsISupports *aOuter, 
                               const nsIID &aIID, 
                               void **aResult ); 
    NS_IMETHOD LockFactory( PRBool aLock ); 
private: 
    nsInstanceCounter instanceCounter; 
}; 
/* nsISupports interface implementation for the factory. */
NS_IMPL_ADDREF( className##Factory ) 
NS_IMPL_RELEASE( className##Factory ) 
NS_IMETHODIMP 
className##Factory::QueryInterface( const nsIID &anIID, void **aResult ) { 
    nsresult rv = NS_OK; 
    if ( aResult ) { 
        *aResult = 0; 
        if ( 0 ) { 
        } else if ( anIID.Equals( NS_GET_IID(nsIFactory) ) ) { 
            *aResult = (void*) (nsIFactory*)this; 
            NS_ADDREF_THIS(); 
        } else if ( anIID.Equals( NS_GET_IID(nsISupports) ) ) { 
            *aResult = (void*) (nsISupports*) (interfaceName*)this; 
            NS_ADDREF_THIS(); 
        } else { 
            rv = NS_ERROR_NO_INTERFACE; 
        } 
    } else { 
        rv = NS_ERROR_NULL_POINTER; 
    } 
    return rv; 
} 
/* Factory's CreateInstance implementation */
NS_IMETHODIMP 
className##Factory::CreateInstance( nsISupports *anOuter, 
                                    const nsIID &anIID, 
                                    void*       *aResult ) { 
    nsresult rv = NS_OK; 
    if ( aResult ) { 
        /* Allocate new find component object. */
        className *component = new className(); 
        if ( component ) { 
            /* Allocated OK, do query interface to get proper */
            /* pointer and increment refcount.                */
            rv = component->QueryInterface( anIID, aResult ); 
            if ( NS_FAILED( rv ) ) { 
                /* refcount still at zero, delete it here. */
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
/* Factory's LockFactory implementation */
NS_IMETHODIMP 
className##Factory::LockFactory(PRBool aLock) { 
      return nsInstanceCounter::LockFactory( aLock ); 
} 
class className##Module : public nsIModule { 
public: 
    className##Module() { 
        NS_INIT_REFCNT(); 
    } 
    virtual ~className##Module() { 
    } 
    NS_DECL_ISUPPORTS 
    NS_DECL_NSIMODULE 
}; 
NS_IMPL_ISUPPORTS1(className##Module, nsIModule) 
/* NSRegisterSelf implementation */
NS_IMETHODIMP 
className##Module::RegisterSelf(nsIComponentManager *compMgr, 
                                nsIFile* aPath,               
                                const char *registryLocation, 
                                const char *componentType) 
{ 
    nsresult rv = NS_OK; 
    /* WARNING: Dont remember service manager. */ 
    /* Get the component manager service. */ 
 
    if (NS_FAILED(rv)) return rv; 
    /* Register our component. */ 
    rv = compMgr->RegisterComponentSpec( className::GetCID(), #className, 
                                          progId, aPath, PR_TRUE, PR_TRUE ); 
    if ( NS_SUCCEEDED( rv ) ) { 
        DEBUG_PRINTF( PR_STDOUT, #className " registration successfuln" ); 
        if ( autoInit ) { 
            /* Add to appshell component list. */ 
            nsIRegistry *registry; 
            rv = nsServiceManager::GetService( NS_REGISTRY_PROGID, 
                                      NS_GET_IID(nsIRegistry), 
                                      (nsISupports**)&registry ); 
            if ( NS_SUCCEEDED( rv ) ) { 
                registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry); 
                char buffer[256]; 
                char *cidString = className::GetCID().ToString(); 
                PR_snprintf( buffer, sizeof buffer, "%s/%s", 
                             NS_IAPPSHELLCOMPONENT_KEY, 
                             cidString ? cidString : "unknown" ); 
                delete [] cidString; 
                nsRegistryKey key; 
                rv = registry->AddSubtree( nsIRegistry::Common, buffer, &key ); 
                if ( NS_SUCCEEDED( rv ) ) { 
                    DEBUG_PRINTF( PR_STDOUT, #className " added to appshell component listn" ); 
                } else { 
                    DEBUG_PRINTF( PR_STDOUT, #className " not added to appshell component list, rv=0x%Xn", (int)rv ); 
                } 
            } else { 
                DEBUG_PRINTF( PR_STDOUT, #className " not added to appshell component list, rv=0x%Xn", (int)rv ); 
            } 
        } 
    } else { 
        DEBUG_PRINTF( PR_STDOUT, #className " registration failed, RegisterComponent rv=0x%Xn", (int)rv ); 
    } 
 
    return rv; 
} 
/* UnregisterSelf implementation */ 
NS_IMETHODIMP 
className##Module::UnregisterSelf( nsIComponentManager *compMgr, 
                                   nsIFile* aPath,               
                                   const char *registryLocation) { 
    nsresult rv = NS_OK; 
    if (NS_FAILED(rv)) 
    { 
        DEBUG_PRINTF( PR_STDOUT, #className " registration failed, GetService rv=0x%Xn", (int)rv ); 
        return rv; 
    } 
 
    /* Unregister our component. */ 
    rv = compMgr->UnregisterComponentSpec( className::GetCID(), aPath); 
    if ( NS_SUCCEEDED( rv ) ) { 
        DEBUG_PRINTF( PR_STDOUT, #className " unregistration successfuln" ); 
    } else { 
        DEBUG_PRINTF( PR_STDOUT, #className " unregistration failed, UnregisterComponent rv=0x%Xn", (int)rv ); 
    } 
 
    return rv; 
} 
/* GetFactory implementation */
NS_IMETHODIMP 
className##Module::GetClassObject( nsIComponentManager *compMgr, 
              const nsCID &aClass, 
              const nsIID &aIID, 
              void **aFactory ) { 
    nsresult rv = NS_OK; 
    if ( NS_SUCCEEDED( rv ) ) { 
        if ( aFactory ) { 
            className##Factory *factory = new className##Factory(); 
            if ( factory ) { 
                NS_ADDREF(factory); 
                rv = factory->QueryInterface( aIID, (void**)aFactory ); 
                NS_RELEASE(factory); 
            } else { 
                rv = NS_ERROR_OUT_OF_MEMORY; 
            } 
        } else { 
            rv = NS_ERROR_NULL_POINTER; 
        } 
    } 
    return rv; 
} 
NS_IMETHODIMP 
className##Module::CanUnload( nsIComponentManager*, PRBool* canUnload) { 
      if (!canUnload) return NS_ERROR_NULL_POINTER; 
      *canUnload = nsInstanceCounter::CanUnload(); 
      return NS_OK; 
} 
#endif
NS_IMPL_IAPPSHELLCOMPONENTIMPL_CTORDTOR( className ) 
#endif
