/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include <stdio.h>

#include "nsAppRunner.h"
#include "nsIFactory.h"

// Define Class IDs.
static NS_DEFINE_IID(kAppRunnerCID, NS_APPRUNNER_CID);

// Define Interface IDs.
static NS_DEFINE_IID(kIAppRunnerIID, NS_IAPPRUNNER_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

// Generate implementation of nsISupports stuff.
NS_IMPL_ISUPPORTS(nsAppRunner, kIAppRunnerIID);

// ctor
nsAppRunner::nsAppRunner() 
    : mArgc( 0 ), mArg( 0 ) {
    NS_INIT_REFCNT();
}

// dtor
nsAppRunner::~nsAppRunner() {
    delete [] mArg;
}

/*---------------------------- nsAppRunner::main -------------------------------
| Initialize, then load and run the app shell.  All work is farmed out to      |
| virtual functions to enable customization.                     |             |
------------------------------------------------------------------------------*/
NS_IMETHODIMP
nsAppRunner::main( int argc, char *argv[] ) {
#if 0
    fprintf( stdout, "nsAppRunner::main entered\n" );

    for ( int i = 0; i < argc; i++ ) {
        fprintf( stdout, "\targv[%d] = %s\n", i, argv[i] );
    }

    return NS_OK;
#else
    nsresult rv = Initialize();

    if ( rv == NS_OK ) {
        // Load app shell.
        rv = LoadAppShell();

        if ( rv == NS_OK ) {
            // Execute the shell.
            rv = AppShell()->Initialize();

            if ( rv == NS_OK ) {
                // Run the shell.
                rv = AppShell()->Run();

                // Shut it down.
                AppShell()->Shutdown();

                if ( rv != NS_OK ) {
                    DisplayMsg( "nsAppRunner error", "app shell failed to run, rv=0x%lX", rv );
                }
            } else {
                DisplayMsg( "nsAppRunner error", "app shell initialization failed, rv=0x%lX", rv );
            }
        } else {
            char *cid = AppShellCID().ToString();
            DisplayMsg( "nsAppRunner error", "Unable to load app shell (CID=%s), rv=0x%lX", cid, rv );
            delete [] cid;
        }
    } else {
        DisplayMsg( "nsAppRunner error", "Initialize failed, rv=0x%lX", rv );
    }

    return rv;
#endif
}


/*---------------------- NSAppRunner::ParseCommandLine -------------------------
| Store the command line arguments in data members with  minor twiddling to    |
| simplify lookup.                                                             |
------------------------------------------------------------------------------*/
PRBool NSAppRunner::ParseCommandLine( int argc, char *argv[] ) { {
    mArgc = argc;

    // Allocate array of arg info structures.
    mArg = new [argc] Arg;

    // Fill in structures.
    for( int i = 0; i < mArgc; i++ ) {
        char *p = argv[i];

        // "Key" is always the beginning
        mArg[i].key = p;

        // Bump pointer till we get to null or double quote.
        while ( *p && *p != '"' ) {
            p++;
        }

        // If not null, get value.
        if ( *p ) { 
            *p++ = 0; // Terminate key and advance pointer.

            // Value starts here.
            mArg[i].value = p;

            // Strip trailing '"'.
            if ( p[ PR_strlen(p) ] == '"' ) {
                p[ PR_strlen(p) ] = 0;
            }
        } else {
            // Have value point to this null string.
            mArg[i].value = p;
        }

    }

    return PR_TRUE;
}


/*---------------------- NSAppRunner::IsOptionSpecified ------------------------
| Enumerate array of arg structures, looking for key.                          |
------------------------------------------------------------------------------*/
PRBool NSAppRunner::IsOptionSpecified( const char *key, PRBool ignoreCase ) {
    PRBool result = PR_FALSE;
    for( int i = 0; i < mArgc; i++ ) {
        if ( ignoreCase ) {
            if ( PR_strcmpi( key, mArg[i].key ) == 0 ) {
                result = PR_TRUE;
                break;
            }
        } else {
            if ( PR_strcmp( key, mArg[i].key ) == 0 ) {
                result = PR_TRUE;
                break;
            }
        }
    }
    return result;
}

/*----------------------- NSAppRunner::*ValueForOption -------------------------
  Look up key
------------------------------------------------------------------------------*/
const char *NSAppRunner::ValueForOption( const char *key, PRBool ignoreCase ) {
    return 0;
}


    // Process management:
    //   Initialize       - Initialize kernel services.  Default is to initialize
    //                      NSPR.
    //   ShowSplashScreen - Puts up splash screen.  Default is to do nothing.
    //                      We will override per platform, probably.
    //   IsSingleInstanceOnly - Default is PR_FALSE, platforms must override
    //                      if they need to block multiple instances.
    //   IsAlreadyRunning - Indicate whether another process is already running.
    //                      Default returns PR_FALSE; platforms should override
    //                      if they care about this (i.e., return PR_TRUE from
    //                      PermitsMultipleInstances).
    //   NotifyServerProcess - Passes request to the server process to be run.
    //   ...synchronization stuff...TBD
    virtual PRBool   Initialize();
    virtual nsresult Exit( nsresult rv );
    virtual PRBool   ShowSplashScreen();
    virtual PRBool   IsSingleInstanceOnly() const;
    virtual PRBool   IsAlreadyRunning() const;
    virtual PRBool   NotifyServerProcess( int argc, char *argv[] );

    // AppShell loading:
    //   AppShellCID  - Returns CID for app shell to be loaded/started.
    //   LoadAppShell - Default creates appShell from ServiceManager using
    //                  AppShellCID().
    //   RunShell     - Initialize/Run/Shutdown the app shell
    //   AppShell     - Returns pointer to app shell
    virtual nsCID        AppShellCID() const;
    virtual nsresult     LoadAppShell();
    virtual nsresult     RunShell();
    virtual nsIAppShell *AppShell() const;

    // Utilities:
    //   DisplayMsg - Displays error/status message.  Default is to outpout to
    //                stderr.  Override to put up message box, etc.
    virtual void DisplayMsg( const char *title, const char *text );
            void DisplayMsg( const char *title, const char *text, ... );
            void DisplayMsg( const char *title, const char *text, valist args );


#if 0
// nsAppRunnerFactory
//
// Means by which instances are created via nsRepository.
// This code is somewhat temporary as the CreateInstance should really
// go in platform-specific code in order to create an instance specific
// to the platform on which we're running.
class nsAppRunnerFactory : public nsIFactory {
public:
    NS_DECL_ISUPPORTS

    nsAppRunnerFactory();

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~nsAppRunnerFactory();
};


nsAppRunnerFactory::nsAppRunnerFactory() {
    NS_INIT_REFCNT();
}

nsAppRunnerFactory::~nsAppRunnerFactory() {
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(nsAppRunnerFactory, kIFactoryIID);

NS_IMETHODIMP
nsAppRunnerFactory::CreateInstance(nsISupports *aOuter,
                                   const nsIID &aIID,
                                   void **aResult) {
    nsresult rv = NS_OK;
    nsAppRunner* newAppRunner;

    if (aResult == NULL) {
        return NS_ERROR_NULL_POINTER;
    } else {
        *aResult = NULL;
    }

    if (0 != aOuter) {
        return NS_ERROR_NO_AGGREGATION;
    }

    NS_NEWXPCOM(newAppRunner, nsAppRunner);

    if (newAppRunner == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(newAppRunner);
    rv = newAppRunner->QueryInterface(aIID, aResult);
    NS_RELEASE(newAppRunner);

    return rv;
}

nsresult
nsAppRunnerFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(const nsCID &cid, nsIFactory** aFactory) {
    nsresult rv = NS_OK;

    if ( aFactory == 0 ) {
        return NS_ERROR_NULL_POINTER;
    } else {
        *aFactory = 0;
    }

    nsIFactory* inst = new nsAppRunnerFactory();
    if (0 == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ADDREF(inst);
        *aFactory = inst;
    }

    return rv;
}
#endif
