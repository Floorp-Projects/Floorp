/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef __nsIAppShellComponentImpl_h
#define __nsIAppShellComponentImpl_h

#include "nsIAppShellComponent.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsICmdLineService.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIRegistry.h"
#include "pratom.h"
#include "nsCOMPtr.h"
#include "prprf.h"

/*------------------------------------------------------------------------------
| This file contains macros to assist in the implementation of application     |
| shell components.  The macros of interest (that you would typically use      |
| in the implementation of your component) are:                                |
|                                                                              |
| NS_IMPL_IAPPSHELLCOMPONENT( className, interfaceName, contractId ):              |
|                                                                              |
|   This macro will generate all the boilderplate app shell component          |
|   implementation details.                                                    |
|       "className" is the name of your implementation                         |
|           class.                                                             |
|       "interfaceName" is the name of the interface that the class            |
|           implements (sorry, currently only one is supported).               |
|       "contractId" is the contractId corresponding to the interface your             |
|           class implements.                                                  |
------------------------------------------------------------------------------*/

#if defined( NS_DEBUG ) && !defined( XP_MAC )
    #define DEBUG_PRINTF PR_fprintf
#else
    #define DEBUG_PRINTF (void)
#endif

#define NS_IMPL_NSGETMODULE1(_class)                                           \
static _class *g##_class;                                                     \
extern "C" NS_EXPORT nsresult                                                 \
NSGetModule_##_class(nsIComponentManager *servMgr,                            \
            nsIFile* aPath,                                                   \
            nsIModule** aResult)                                              \
{                                                                             \
    nsresult rv = NS_OK;                                                      \
                                                                              \
    if (!aResult) return NS_ERROR_NULL_POINTER;                               \
    if (!g##_class) {                                                         \
      g##_class = new _class;                                                 \
      if (!g##_class) return NS_ERROR_OUT_OF_MEMORY;                          \
    }                                                                         \
                                                                              \
    NS_ADDREF(g##_class);                                                     \
    if (g##_class)                                                            \
      rv = g##_class->QueryInterface(NS_GET_IID(nsIModule),                   \
                                   (void **)aResult);                         \
    NS_RELEASE(g##_class);                                                    \
    return rv;                                                                \
}

// Declare nsInstanceCounter class.
class nsInstanceCounter {
public:
    // ctor increments (shared library) global instance counter.
    nsInstanceCounter() {
#if defined (DYNLINKED_MODULES)
        PR_AtomicIncrement( &mInstanceCount );
#endif /* defined (DYNLINKED_MODULES) */
    }
    // dtor decrements it.
    ~nsInstanceCounter() {
#if defined (DYNLINKED_MODULES)
        PR_AtomicDecrement( &mInstanceCount );
#endif /* defined (DYNLINKED_MODULES) */
    }
    // Tests whether shared library can be unloaded.
    static PRBool CanUnload() {
#if defined (DYNLINKED_MODULES)
        return mInstanceCount == 0 && mLockCount == 0;
#else /* !defined (DYNLINKED_MODULES) */
	return 0;
#endif /* !defined (DYNLINKED_MODULES) */
    }
    // Lock/unlock (a factory).
    static nsresult LockFactory( PRBool aLock ) {
#if defined (DYNLINKED_MODULES)
        if (aLock)
            PR_AtomicIncrement( &mLockCount ); 
        else
            PR_AtomicDecrement( &mLockCount );
#endif /* defined (DYNLINKED_MODULES) */
        return NS_OK;
    }
#if defined (DYNLINKED_MODULES)
private:
    static PRInt32 mInstanceCount, mLockCount;
#endif /* defined (DYNLINKED_MODULES) */
}; // nsInstanceCounter

#if defined (DYNLINKED_MODULES)
#define NS_DEFINE_MODULE_INSTANCE_COUNTER() \
PRInt32 nsInstanceCounter::mInstanceCount = 0; \
PRInt32 nsInstanceCounter::mLockCount = 0;
#else /* !defined (DYNLINKED_MODULES) */
#define NS_DEFINE_MODULE_INSTANCE_COUNTER()
#endif /* !defined (DYNLINKED_MODULES) */

// Declare component-global class.
class nsAppShellComponentImpl {
public:
    // Override this and return PR_FALSE if your component
    // should not be registered with the Service Manager
    // when Initialize-d.
    virtual PRBool Is_Service() { return PR_TRUE; }
    // Override this to perform initialization for your component.
    NS_IMETHOD DoInitialization() { return NS_OK; }
    static nsIAppShellService *GetAppShell() {
#if defined (DYNLINKED_MODULES)
        if ( !mAppShell )
#else /* !defined (DYNLINKED_MODULES) */
	nsIAppShellService *mAppShell = 0;
#endif /* !defined (DYNLINKED_MODULES) */
	{
            nsCID cid = NS_APPSHELL_SERVICE_CID;
            nsServiceManager::GetService( cid,
                                         NS_GET_IID(nsIAppShellService),
                                         (nsISupports**)&mAppShell );
        }
        return mAppShell;
    }
#if defined (DYNLINKED_MODULES)
    static nsIAppShellService *mAppShell;
    static nsICmdLineService  *mCmdLine;
#endif /* !defined (DYNLINKED_MODULES) */
    #ifdef NS_DEBUG
    nsAppShellComponentImpl();
    virtual ~nsAppShellComponentImpl();
    #endif
}; // nsAppShellComponent

#if defined (DYNLINKED_MODULES)
#define NS_DEFINE_COMPONENT_GLOBALS() \
nsIAppShellService *nsAppShellComponentImpl::mAppShell   = 0; \
nsICmdLineService  *nsAppShellComponentImpl::mCmdLine    = 0;
#define SAVE_STUFF \
   mAppShell = anAppShell; \
   mCmdLine  = aCmdLineService;
#else /* !defined (DYNLINKED_MODULES) */
#define NS_DEFINE_COMPONENT_GLOBALS()
#define SAVE_STUFF
#endif /* !defined (DYNLINKED_MODULES) */

// Macros to define ctor/dtor for implementation class.
// These differ in debug vs. non-debug situations.
#ifdef NS_DEBUG
#define NS_IMPL_IAPPSHELLCOMPONENTIMPL_CTORDTOR(className) \
nsAppShellComponentImpl::nsAppShellComponentImpl() { \
    DEBUG_PRINTF( PR_STDOUT, #className " component created\n" ); \
} \
nsAppShellComponentImpl::~nsAppShellComponentImpl() { \
    DEBUG_PRINTF( PR_STDOUT, #className " component destroyed\n" ); \
}
#else
#define NS_IMPL_IAPPSHELLCOMPONENTIMPL_CTORDTOR(className)
#endif

#define NS_IMPL_IAPPSHELLCOMPONENT( className, interfaceName, contractId, autoInit ) \
/* Define instance counter implementation stuff. */\
NS_DEFINE_MODULE_INSTANCE_COUNTER() \
/* Define component globals. */\
NS_DEFINE_COMPONENT_GLOBALS() \
/* Component's implementation of Initialize. */\
NS_IMETHODIMP \
className::Initialize( nsIAppShellService *anAppShell, \
                       nsICmdLineService  *aCmdLineService ) { \
    nsresult rv = NS_OK; \
    SAVE_STUFF \
    if ( Is_Service() ) { \
        rv = nsServiceManager::RegisterService( contractId, this ); \
    } \
    if ( NS_SUCCEEDED( rv ) ) { \
        rv = DoInitialization(); \
    } \
    return rv; \
} \
/* Component's implementation of Shutdown. */\
NS_IMETHODIMP \
className::Shutdown() { \
    nsresult rv = NS_OK; \
    if ( Is_Service() ) { \
        rv = nsServiceManager::UnregisterService( contractId ); \
    } \
    return rv; \
} \
/* nsISupports Implementation for the class */\
NS_IMPL_ADDREF( className );  \
NS_IMPL_RELEASE( className ); \
/* QueryInterface implementation for this class. */\
NS_IMETHODIMP \
className::QueryInterface( REFNSIID anIID, void **anInstancePtr ) { \
    nsresult rv = NS_OK; \
    /* Check for place to return result. */\
    if ( !anInstancePtr ) { \
        rv = NS_ERROR_NULL_POINTER; \
    } else { \
        /* Initialize result. */\
        *anInstancePtr = 0; \
        /* Check for IIDs we support and cast this appropriately. */\
        if ( 0 ) { \
        } else if ( anIID.Equals( NS_GET_IID(interfaceName) ) ) { \
            *anInstancePtr = (void*) this; \
            NS_ADDREF_THIS(); \
        } else if ( anIID.Equals( NS_GET_IID(nsIAppShellComponent) ) ) { \
            *anInstancePtr = (void*) ( (nsIAppShellComponent*)this ); \
            NS_ADDREF_THIS(); \
        } else if ( anIID.Equals( NS_GET_IID(nsISupports) ) ) { \
            *anInstancePtr = (void*) ( (nsISupports*)this ); \
            NS_ADDREF_THIS(); \
        } else { \
            /* Not an interface we support. */\
            rv = NS_NOINTERFACE; \
        } \
    } \
    return rv; \
} \
/* Factory class */\
struct className##Factory : public nsIFactory { \
    /* ctor/dtor */\
    className##Factory() { \
        NS_INIT_REFCNT(); \
    } \
    virtual ~className##Factory() { \
    } \
    /* This class implements the nsISupports interface functions. */\
	NS_DECL_ISUPPORTS \
    /* nsIFactory methods */\
    NS_IMETHOD CreateInstance( nsISupports *aOuter, \
                               const nsIID &aIID, \
                               void **aResult ); \
    NS_IMETHOD LockFactory( PRBool aLock ); \
private: \
    nsInstanceCounter instanceCounter; \
}; \
/* nsISupports interface implementation for the factory. */\
NS_IMPL_ADDREF( className##Factory ) \
NS_IMPL_RELEASE( className##Factory ) \
NS_IMETHODIMP \
className##Factory::QueryInterface( const nsIID &anIID, void **aResult ) { \
    nsresult rv = NS_OK; \
    if ( aResult ) { \
        *aResult = 0; \
        if ( 0 ) { \
        } else if ( anIID.Equals( NS_GET_IID(nsIFactory) ) ) { \
            *aResult = (void*) (nsIFactory*)this; \
            NS_ADDREF_THIS(); \
        } else if ( anIID.Equals( NS_GET_IID(nsISupports) ) ) { \
            *aResult = (void*) (nsISupports*)this; \
            NS_ADDREF_THIS(); \
        } else { \
            rv = NS_ERROR_NO_INTERFACE; \
        } \
    } else { \
        rv = NS_ERROR_NULL_POINTER; \
    } \
    return rv; \
} \
/* Factory's CreateInstance implementation */\
NS_IMETHODIMP \
className##Factory::CreateInstance( nsISupports *anOuter, \
                                    const nsIID &anIID, \
                                    void*       *aResult ) { \
    nsresult rv = NS_OK; \
    if ( aResult ) { \
        /* Allocate new find component object. */\
        className *component = new className(); \
        if ( component ) { \
            /* Allocated OK, do query interface to get proper */\
            /* pointer and increment refcount.                */\
            rv = component->QueryInterface( anIID, aResult ); \
            if ( NS_FAILED( rv ) ) { \
                /* refcount still at zero, delete it here. */\
                delete component; \
            } \
        } else { \
            rv = NS_ERROR_OUT_OF_MEMORY; \
        } \
    } else { \
        rv = NS_ERROR_NULL_POINTER; \
    } \
    return rv; \
} \
/* Factory's LockFactory implementation */\
NS_IMETHODIMP \
className##Factory::LockFactory(PRBool aLock) { \
      return nsInstanceCounter::LockFactory( aLock ); \
} \
class className##Module : public nsIModule { \
public: \
    className##Module() { \
        NS_INIT_REFCNT(); \
    } \
    virtual ~className##Module() { \
    } \
    NS_DECL_ISUPPORTS \
    NS_DECL_NSIMODULE \
}; \
NS_IMPL_ISUPPORTS1(className##Module, nsIModule) \
/* NSRegisterSelf implementation */\
NS_IMETHODIMP \
className##Module::RegisterSelf(nsIComponentManager *compMgr, \
                                nsIFile* aPath,               \
                                const char *registryLocation, \
                                const char *componentType) \
{ \
    nsresult rv = NS_OK; \
    /* WARNING: Dont remember service manager. */ \
    /* Get the component manager service. */ \
 \
    if (NS_FAILED(rv)) return rv; \
    /* Register our component. */ \
    rv = compMgr->RegisterComponentSpec( className::GetCID(), #className, \
                                          contractId, aPath, PR_TRUE, PR_TRUE ); \
    if ( NS_SUCCEEDED( rv ) ) { \
        DEBUG_PRINTF( PR_STDOUT, #className " registration successful\n" ); \
        if ( autoInit ) { \
            /* Add to appshell component list. */ \
            nsIRegistry *registry; \
            rv = nsServiceManager::GetService( NS_REGISTRY_CONTRACTID, \
                                      NS_GET_IID(nsIRegistry), \
                                      (nsISupports**)&registry ); \
            if ( NS_SUCCEEDED( rv ) ) { \
                registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry); \
                char buffer[256]; \
                char *cidString = className::GetCID().ToString(); \
                PR_snprintf( buffer, sizeof buffer, "%s/%s", \
                             NS_IAPPSHELLCOMPONENT_KEY, \
                             cidString ? cidString : "unknown" ); \
                delete [] cidString; \
                nsRegistryKey key; \
                rv = registry->AddSubtree( nsIRegistry::Common, buffer, &key ); \
                if ( NS_SUCCEEDED( rv ) ) { \
                    DEBUG_PRINTF( PR_STDOUT, #className " added to appshell component list\n" ); \
                } else { \
                    DEBUG_PRINTF( PR_STDOUT, #className " not added to appshell component list, rv=0x%X\n", (int)rv ); \
                } \
            } else { \
                DEBUG_PRINTF( PR_STDOUT, #className " not added to appshell component list, rv=0x%X\n", (int)rv ); \
            } \
        } \
    } else { \
        DEBUG_PRINTF( PR_STDOUT, #className " registration failed, RegisterComponent rv=0x%X\n", (int)rv ); \
    } \
 \
    return rv; \
} \
/* UnregisterSelf implementation */ \
NS_IMETHODIMP \
className##Module::UnregisterSelf( nsIComponentManager *compMgr, \
                                   nsIFile* aPath,               \
                                   const char *registryLocation) { \
    nsresult rv = NS_OK; \
    if (NS_FAILED(rv)) \
    { \
        DEBUG_PRINTF( PR_STDOUT, #className " registration failed, GetService rv=0x%X\n", (int)rv ); \
        return rv; \
    } \
 \
    /* Unregister our component. */ \
    rv = compMgr->UnregisterComponentSpec( className::GetCID(), aPath); \
    if ( NS_SUCCEEDED( rv ) ) { \
        DEBUG_PRINTF( PR_STDOUT, #className " unregistration successful\n" ); \
    } else { \
        DEBUG_PRINTF( PR_STDOUT, #className " unregistration failed, UnregisterComponent rv=0x%X\n", (int)rv ); \
    } \
 \
    return rv; \
} \
/* GetFactory implementation */\
NS_IMETHODIMP \
className##Module::GetClassObject( nsIComponentManager *compMgr, \
              const nsCID &aClass, \
              const nsIID &aIID, \
              void **aFactory ) { \
    nsresult rv = NS_OK; \
    if ( NS_SUCCEEDED( rv ) ) { \
        if ( aFactory ) { \
            className##Factory *factory = new className##Factory(); \
            if ( factory ) { \
                NS_ADDREF(factory); \
                rv = factory->QueryInterface( aIID, (void**)aFactory ); \
                NS_RELEASE(factory); \
            } else { \
                rv = NS_ERROR_OUT_OF_MEMORY; \
            } \
        } else { \
            rv = NS_ERROR_NULL_POINTER; \
        } \
    } \
    return rv; \
} \
NS_IMETHODIMP \
className##Module::CanUnload( nsIComponentManager*, PRBool* canUnload) { \
      if (!canUnload) return NS_ERROR_NULL_POINTER; \
      *canUnload = nsInstanceCounter::CanUnload(); \
      return NS_OK; \
} \
NS_IMPL_IAPPSHELLCOMPONENTIMPL_CTORDTOR( className ) \
NS_IMPL_NSGETMODULE1(className##Module)
#endif
