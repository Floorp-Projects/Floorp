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
#include <stdarg.h>

#include "nsAppRunner.h"
#include "nsIAppShellService.h"
#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "xp_core.h"
#include "xp_mcom.h"
#include "xp_str.h"

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
PRBool nsAppRunner::ParseCommandLine( int argc, char *argv[] ) {
    mArgc = argc;

    // Allocate array of arg info structures.
    mArg = new Arg[ mArgc ];

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
            if ( p[ XP_STRLEN(p) ] == '"' ) {
                p[ XP_STRLEN(p) ] = 0;
            }
        } else {
            // Have value point to this null string.
            mArg[i].value = p;
        }

    }

    return PR_TRUE;
}


/*------------------------- nsAppRunner::IndexOfArg ----------------------------
| Return index of specified argument "key."  -1 if not found.                  |
------------------------------------------------------------------------------*/
int nsAppRunner::IndexOfArg( const char *key, PRBool ignoreCase ) {
    int result = -1;
    for( int i = 0; result == -1 && i < mArgc; i++ ) {
        if ( ( ignoreCase && XP_STRCASECMP( key, mArg[i].key ) == 0 )
             ||
             ( !ignoreCase && XP_STRCMP( key, mArg[i].key ) == 0 ) ) {
            result = i;
        }
    }
    return result;
}


/*---------------------- nsAppRunner::IsOptionSpecified ------------------------
| Search for key via IndexOfArg and return true iff it is found.               |
------------------------------------------------------------------------------*/
PRBool nsAppRunner::IsOptionSpecified( const char *key, PRBool ignoreCase ) {
    int index = this->IndexOfArg( key, ignoreCase );
    return index != -1;
}

/*----------------------- nsAppRunner::*ValueForOption -------------------------
| Search for key via IndexOfArg and return corresponding value if it is found, |
| 0 otherwise.                                                                 |
------------------------------------------------------------------------------*/
const char *nsAppRunner::ValueForOption( const char *key, PRBool ignoreCase ) {
    const char *result = 0;
    int index = this->IndexOfArg( key, ignoreCase );
    if ( index != -1 ) {
        result = mArg[ index ].value;
    }
    return result;
}


/*------------------------- nsAppRunner::Initialize ----------------------------
| Default simply returns NS_OK.                                                |
------------------------------------------------------------------------------*/
nsresult nsAppRunner::Initialize() {
    return NS_OK;
}


/*--------------------------- nsAppRunner::OnExit ------------------------------
| Default simply returns the input application return value unmodified.        |
------------------------------------------------------------------------------*/
nsresult nsAppRunner::OnExit( nsresult rvIn ) {
    return rvIn;
}


/*------------------------- nsAppRunner::AppShellCID ---------------------------
| Return the CID for nsAppShell.  Long term, get this ID dynamically?          |
------------------------------------------------------------------------------*/
nsCID nsAppRunner::AppShellCID() const {
    nsCID appShellCID = NS_APPSHELL_SERVICE_CID;
    return appShellCID;
}


/*------------------------ nsAppRunner::LoadAppShell ---------------------------
| Create an instance of the app shell (object of class represented by          |
| the CID returned by AppShellCID).                                            |
------------------------------------------------------------------------------*/
nsresult nsAppRunner::LoadAppShell() {
    nsIAppShellService *appShell = 0;
    nsIID kIAppShellServiceIID = NS_IAPPSHELL_SERVICE_IID;
    nsresult rv = nsServiceManager::GetService( this->AppShellCID(),
                                                kIAppShellServiceIID,
                                                (nsISupports**)&appShell );
    if ( rv == NS_OK ) {
        this->SetAppShell( appShell );
        NS_IF_RELEASE( appShell );
    }

    return rv;
}


/*-------------------------- nsAppRunner::RunShell -----------------------------
| Run the app shell.                                                           |
------------------------------------------------------------------------------*/
nsresult nsAppRunner::RunShell() {
    nsresult result = this->mAppShell->Run();
    return result;
}


/*-------------------------- nsAppRunner::*AppShell ----------------------------
| Return the app shell associated with this app runner.                        |
------------------------------------------------------------------------------*/
nsIAppShellService *nsAppRunner::AppShell() const {
    return mAppShell;
}


/*------------------------- nsAppRunner::DisplayMsg ----------------------------
| Write out the title and text to stderr.                                      |
------------------------------------------------------------------------------*/
void nsAppRunner::DisplayMsg( const char *title, const char *text ) {
    fprintf( stderr, "%s\n", title );
    fprintf( stderr, "%s\n", text );
    return;
}


/*------------------------- nsAppRunner::DisplayMsg ----------------------------
| Use svprintf to expand the text+args then display the complete text using    |
| the overloaded version of nsAppRunner::DisplayMsg().                         |
------------------------------------------------------------------------------*/
static void displayMsg( nsAppRunner &obj, const char *title, const char *text, va_list args ) {
    // Fix this later.
    obj.DisplayMsg( title, text );
    return;
}


/*------------------------- nsAppRunner::DisplayMsg ----------------------------
| Convert the extra arguments to a vargs list and invoke the static function   |
| that takes one of those.                                                     |
------------------------------------------------------------------------------*/
void nsAppRunner::DisplayMsg( const char *title, const char *text, ... ) {
    va_list args;
    va_start( text, args );
    this->DisplayMsg( title, text, args );
    va_end( args );
    return;
}


/*------------------------ nsAppRunner::&SetAppShell ---------------------------
| Simply do as directed, return *this.                                         |
------------------------------------------------------------------------------*/
nsAppRunner &nsAppRunner::SetAppShell( nsIAppShellService *newAppShell ) {
    // Bump refcount for new shell.
    newAppShell->AddRef();

    // Release current app shell.
    NS_IF_RELEASE(this->mAppShell);

    // Attach new one.
    this->mAppShell = newAppShell;

    return *this;
}


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
