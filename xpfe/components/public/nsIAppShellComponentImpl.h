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
#include "pratom.h"
#include "nsCOMPtr.h"
#ifdef NS_DEBUG
#include "prprf.h"
#endif

/*------------------------------------------------------------------------------
| This file contains macros to assist in the implementation of application     |
| shell components.  The macros of interest (that you would typically use      |
| in the implementation of your component) are:                                |
|                                                                              |
| NS_IMPL_IAPPSHELLCOMPONENT( className, interfaceName, progId ):              |
|                                                                              |
|   This macro will generate all the boilderplate app shell component          |
|   implementation details.                                                    |
|       "className" is the name of your implementation                         |
|           class.                                                             |
|       "interfaceName" is the name of the interface that the class            |
|           implements (sorry, currently only one is supported).               |
|       "progId" is the progId corresponding to the interface your             |
|           class implements.                                                  |
------------------------------------------------------------------------------*/

#ifdef NS_DEBUG
    #define DEBUG_PRINTF PR_fprintf
#else
    #define DEBUG_PRINTF (void)
#endif

// Declare nsInstanceCounter class.
class nsInstanceCounter {
public:
    // ctor increments (shared library) global instance counter.
    nsInstanceCounter() {
        PR_AtomicIncrement( &mInstanceCount );
    }
    // dtor decrements it.
    ~nsInstanceCounter() {
        PR_AtomicIncrement( &mInstanceCount );
    }
    // Tests whether shared library can be unloaded.
    static PRBool CanUnload() {
        return mInstanceCount == 0 && mLockCount == 0;
    }
    // Lock/unlock (a factory).
    static nsresult LockFactory( PRBool lock ) {
        if (lock)
            PR_AtomicIncrement( &mLockCount ); 
        else
            PR_AtomicDecrement( &mLockCount );
        return NS_OK;
    }
private:
    static PRInt32 mInstanceCount, mLockCount;
}; // nsInstanceCounter

#define NS_DEFINE_MODULE_INSTANCE_COUNTER() \
PRInt32 nsInstanceCounter::mInstanceCount = 0; \
PRInt32 nsInstanceCounter::mLockCount = 0;

#define NS_IMPL_IAPPSHELLCOMPONENT( className, interfaceName, progId ) \
/* Define instance counter implementation stuff. */\
NS_DEFINE_MODULE_INSTANCE_COUNTER() \
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
        } else if ( anIID.Equals( interfaceName::GetIID() ) ) { \
            *anInstancePtr = (void*) this; \
            NS_ADDREF_THIS(); \
        } else if ( anIID.Equals( nsIAppShellComponent::GetIID() ) ) { \
            *anInstancePtr = (void*) ( (nsIAppShellComponent*)this ); \
            NS_ADDREF_THIS(); \
        } else if ( anIID.Equals( nsISupports::GetIID() ) ) { \
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
        } else if ( anIID.Equals( nsIFactory::GetIID() ) ) { \
            *aResult = (void*) (nsIFactory*)this; \
            NS_ADDREF_THIS(); \
        } else if ( anIID.Equals( nsISupports::GetIID() ) ) { \
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
/* NSRegisterSelf implementation */\
extern "C" NS_EXPORT nsresult \
NSRegisterSelf( nsISupports* aServiceMgr, const char* path ) { \
    nsresult rv = NS_OK; \
    nsCOMPtr<nsIServiceManager> serviceMgr( do_QueryInterface( aServiceMgr, &rv ) ); \
    if ( NS_SUCCEEDED( rv ) ) { \
        /* Get the component manager service. */\
        nsCID cid = NS_COMPONENTMANAGER_CID; \
        nsIComponentManager *componentMgr = 0; \
        rv = serviceMgr->GetService( cid, \
                                     nsIComponentManager::GetIID(), \
                                     (nsISupports**)&componentMgr ); \
        if ( NS_SUCCEEDED( rv ) ) { \
            /* Register our component. */\
            rv = componentMgr->RegisterComponent( className::GetCID(), \
                                                  #className, \
                                                  progId, \
                                                  path, \
                                                  PR_TRUE, \
                                                  PR_TRUE ); \
            if ( NS_SUCCEEDED( rv ) ) { \
                DEBUG_PRINTF( PR_STDOUT, #className " registeration successful\n" ); \
            } else { \
                DEBUG_PRINTF( PR_STDOUT, #className " registration failed, RegisterComponent rv=0x%X\n", (int)rv ); \
            } \
            /* Release the component manager service. */\
            serviceMgr->ReleaseService( cid, componentMgr ); \
        } else { \
            DEBUG_PRINTF( PR_STDOUT, #className " registration failed, GetService rv=0x%X\n", (int)rv ); \
        } \
    } else { \
        DEBUG_PRINTF( PR_STDOUT, #className " registration failed, QueryInterface rv=0x%X\n", (int)rv ); \
    } \
    return rv; \
} \
/* NSUnregisterSelf implementation */\
extern "C" NS_EXPORT nsresult \
NSUnregisterSelf( nsISupports* aServiceMgr, const char* path ) { \
    nsresult rv = NS_OK; \
    nsCOMPtr<nsIServiceManager> serviceMgr( do_QueryInterface( aServiceMgr, &rv ) ); \
    if ( NS_SUCCEEDED( rv ) ) { \
        /* Get the component manager service. */\
        nsCID cid = NS_COMPONENTMANAGER_CID; \
        nsIComponentManager *componentMgr = 0; \
        rv = serviceMgr->GetService( cid, \
                                     nsIComponentManager::GetIID(), \
                                     (nsISupports**)&componentMgr ); \
        if ( NS_SUCCEEDED( rv ) ) { \
            /* Register our component. */\
            rv = componentMgr->UnregisterComponent( className::GetCID(), path ); \
            if ( NS_SUCCEEDED( rv ) ) { \
                DEBUG_PRINTF( PR_STDOUT, #className " unregistration successful\n" ); \
            } else { \
                DEBUG_PRINTF( PR_STDOUT, #className " unregistration failed, UnregisterComponent rv=0x%X\n", (int)rv ); \
            } \
            /* Release the component manager service. */\
            serviceMgr->ReleaseService( cid, componentMgr ); \
        } else { \
            DEBUG_PRINTF( PR_STDOUT, #className, " unregistration failed, GetService rv=0x%X\n", (int)rv ); \
        } \
    } else { \
        DEBUG_PRINTF( PR_STDOUT, #className " unregistration failed, QueryInterface rv=0x%X\n", (int)rv ); \
    } \
    return rv; \
} \
/* NSGetFactory implementation */\
extern "C" NS_EXPORT nsresult \
NSGetFactory( nsISupports *aServMgr, \
              const nsCID &aClass, \
              const char  *aClassName, \
              const char  *aProgID, \
              nsIFactory* *aFactory ) { \
    nsresult rv = NS_OK; \
    if ( aFactory ) { \
        className##Factory *factory = new className##Factory(); \
        if ( factory ) { \
            rv = factory->QueryInterface( nsIFactory::GetIID(), (void**)aFactory ); \
            if ( NS_FAILED( rv ) ) { \
                DEBUG_PRINTF( PR_STDOUT, #className " NSGetFactory failed, QueryInterface rv=0x%X\n", (int)rv ); \
                /* Delete this bogus factory. */\
                delete factory; \
            } \
        } else { \
            rv = NS_ERROR_OUT_OF_MEMORY; \
        } \
    } else { \
        rv = NS_ERROR_NULL_POINTER; \
    } \
    return rv; \
} \
/* NSCanUnload implementation */\
extern "C" NS_EXPORT PRBool \
NSCanUnload( nsISupports* aServiceMgr ) { \
      return nsInstanceCounter::CanUnload(); \
}

#endif
