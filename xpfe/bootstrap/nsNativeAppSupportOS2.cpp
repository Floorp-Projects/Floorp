/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Bill Law       law@netscape.com
 *   IBM Corp.
 */
#include "nsNativeAppSupportBase.h"
#include "nsNativeAppSupportOS2.h"
#include "nsString.h"
#include "nsICmdLineService.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIDOMWindowInternal.h"
#define INCL_PM
#define INCL_GPI
#define INCL_DOS
#include <os2.h>
//#include <ddeml.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

// Define this macro to 1 to get DDE debugging output.
#define MOZ_DEBUG_DDE 0

#ifdef DEBUG_law
#undef MOZ_DEBUG_DDE
#define MOZ_DEBUG_DDE 1
#endif

class nsSplashScreenOS2 : public nsISplashScreen {
public:
    nsSplashScreenOS2();
    ~nsSplashScreenOS2();

    NS_IMETHOD Show();
    NS_IMETHOD Hide();

    // nsISupports methods
    NS_IMETHOD_(nsrefcnt) AddRef() {
        mRefCnt++;
        return mRefCnt;
    }
    NS_IMETHOD_(nsrefcnt) Release() {
        --mRefCnt;
        if ( !mRefCnt ) {
            delete this;
            return 0;
        }
        return mRefCnt;
    }
    NS_IMETHOD QueryInterface( const nsIID &iid, void**p ) {
        nsresult rv = NS_OK;
        if ( p ) {
            *p = 0;
            if ( iid.Equals( NS_GET_IID( nsISplashScreen ) ) ) {
                nsISplashScreen *result = this;
                *p = result;
                NS_ADDREF( result );
            } else if ( iid.Equals( NS_GET_IID( nsISupports ) ) ) {
                nsISupports *result = NS_STATIC_CAST( nsISupports*, this );
                *p = result;
                NS_ADDREF( result );
            } else {
                rv = NS_NOINTERFACE;
            }
        } else {
            rv = NS_ERROR_NULL_POINTER;
        }
        return rv;
    }

    void SetDialog( HWND dlg );
    void LoadBitmap();
    static nsSplashScreenOS2* GetPointer( HWND dlg );

    HWND mDlg;
    HBITMAP mBitmap;
    nsrefcnt mRefCnt;
}; // class nsSplashScreenOS2

MRESULT EXPENTRY DialogProc( HWND dlg, ULONG msg, MPARAM mp1, MPARAM mp2 );
void _Optlink ThreadProc (void *splashScreen);

#ifndef XP_OS2
// Simple Win32 mutex wrapper.
struct Mutex {
    Mutex( const char *name )
        : mName( name ),
          mHandle( 0 ),
          mState( -1 ) {
        mHandle = CreateMutex( 0, FALSE, mName.GetBuffer() );
        #if MOZ_DEBUG_DDE
        printf( "CreateMutex error = 0x%08X\n", (int)GetLastError() );
        #endif
    }
    ~Mutex() {
        if ( mHandle ) {
            // Make sure we release it if we own it.
            Unlock();

            BOOL rc = CloseHandle( mHandle );
            #if MOZ_DEBUG_DDE
            if ( !rc ) {
                printf( "CloseHandle error = 0x%08X\n", (int)GetLastError() );
            }
            #endif
        }
    }
    BOOL Lock( DWORD timeout ) {
        if ( mHandle ) {
            #if MOZ_DEBUG_DDE
            printf( "Waiting (%d msec) for DDE mutex...\n", (int)timeout );
            #endif
            mState = WaitForSingleObject( mHandle, timeout );
            #if MOZ_DEBUG_DDE
            printf( "...wait complete, result = 0x%08X\n", (int)mState );
            #endif
            return mState == WAIT_OBJECT_0;
        } else {
            return FALSE;
        }
    }
    void Unlock() {
        if ( mHandle && mState == WAIT_OBJECT_0 ) {
            #if MOZ_DEBUG_DDE
            printf( "Releasing DDE mutex\n" );
            #endif
            ReleaseMutex( mHandle );
            mState = -1;
        }
    }
private:
    nsCString mName;
    int       mHandle;
    DWORD     mState;
};
#endif

/* DDE Notes
 *
 * This section describes the Win32 DDE service implementation for
 * Mozilla.  DDE is used on Win32 platforms to communicate between
 * separate instances of mozilla.exe (or other Mozilla-based
 * executables), or, between the Win32 desktop shell and Mozilla.
 *
 * The first instance of Mozilla will become the "server" and
 * subsequent executables (and the shell) will use DDE to send
 * requests to that process.  The requests are DDE "execute" requests
 * that pass the command line arguments.
 *
 * Mozilla registers the DDE application "Mozilla" and currently
 * supports only the "WWW_OpenURL" topic.  This should be reasonably
 * compatible with applications that interfaced with Netscape
 * Communicator (and its predecessors?).  Note that even that topic
 * may not be supported in a compatible fashion as the command-line
 * options for Mozilla are different than for Communiator.
 *
 * It is imperative that at most one instance of Mozilla execute in
 * "server mode" at any one time.  The "native app support" in Mozilla
 * on Win32 ensures that only the server process performs XPCOM
 * initialization (that is not required for subsequent client processes
 * to communicate with the server process).
 *
 * To guarantee that only one server starts up, a Win32 "mutex" is used
 * to ensure only one process executes the server-detection code.  That
 * code consists of initializing DDE and doing a DdeConnect to Mozilla's
 * application/topic.  If that connection succeeds, then a server process
 * must be running already.
 * 
 * Otherwise, no server has started.  In that case, the current process
 * calls DdeNameService to register that application/topic.  Only at that
 * point does the mutex get released.
 *
 * There are a couple of subtleties that one should be aware of:
 * 
 * 1. It is imperative that DdeInitialize be called only after the mutex
 *    lock has been obtained.  The reason is that at shutdown, DDE
 *    notifications go out to all initialized DDE processes.  Thus, if
 *    the mutex is owned by a terminating intance of Mozilla, then
 *    calling DdeInitialize and then WaitForSingleObject will cause the
 *    DdeUninitialize from the terminating process to "hang" until the
 *    process waiting for the mutex times out (and can then service the
 *    notification that the DDE server is terminating).  So, don't mess
 *    with the sequence of things in the startup/shutdown logic.
 *
 * 2. All mutex requests are made with a reasonably long timeout value and
 *    are designed to "fail safe" (i.e., a timeout is treated as failure).
 * 
 * 3. An attempt has been made to minimize the degree to which the main
 *    Mozilla application logic needs to be aware of the DDE mechanisms
 *    implemented herein.  As a result, this module surfaces a very
 *    large-grained interface, consisting of simple start/stop methods.
 *    As a consequence, details of certain scenarios can be "lost."
 *    Particularly, incoming DDE requests can arrive after this module
 *    initiates the DDE server, but before Mozilla is initialized to the
 *    point where those requests can be serviced (e.g., open a browser
 *    window to a particular URL).  Since the client process sends the
 *    request early on, it may not be prepared to respond to that error.
 *    Thus, such situations may fail silently.  The design goal is that
 *    they fail harmlessly.  Refinements on this point will be made as
 *    details emerge (and time permits).
 */
class nsNativeAppSupportOS2 : public nsNativeAppSupportBase {
#ifndef XP_OS2
public:
    // Overrides of base implementation.
    NS_IMETHOD Start( PRBool *aResult );
    NS_IMETHOD Stop( PRBool *aResult );
    NS_IMETHOD Quit();

    // Utility function to handle a Win32-specific command line
    // option: "-console", which dynamically creates a Windows
    // console.
    static void CheckConsole();

private:
    static HDDEDATA CALLBACK HandleDDENotification( UINT     uType,
                                                    UINT     uFmt,
                                                    HCONV    hconv,
                                                    HSZ      hsz1,
                                                    HSZ      hsz2,
                                                    HDDEDATA hdata,
                                                    ULONG    dwData1,
                                                    ULONG    dwData2 );
    static void HandleRequest( LPBYTE request );
    static nsresult GetCmdLineArgs( LPBYTE request, nsICmdLineService **aResult );
    static nsresult OpenWindow( const char *urlstr, const char *args );
    static int   mConversations;
    static HSZ   mApplication, mTopic;
    static DWORD mInstance;
#endif
}; // nsNativeAppSupportOS2

nsSplashScreenOS2::nsSplashScreenOS2()
    : mDlg( 0 ), mBitmap( 0 ), mRefCnt( 0 ) {
}

nsSplashScreenOS2::~nsSplashScreenOS2() {
#if MOZ_DEBUG_DDE
    printf( "splash screen dtor called\n" );
#endif
    // Make sure dialog is gone.
    Hide();
}

NS_IMETHODIMP
nsSplashScreenOS2::Show() {
    //Spawn new thread to display real splash screen.
    int handle = _beginthread( ThreadProc, NULL, 16384, (void *)this );
    return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenOS2::Hide() {
    if ( mDlg ) {
        // Dismiss the dialog.
        WinDismissDlg (mDlg, TRUE);
        // Release custom bitmap (if there is one).
        if ( mBitmap ) {
            BOOL ok = GpiDeleteBitmap( mBitmap );
        }
        mBitmap = 0;
        mDlg = 0;
    }
    return NS_OK;
}

void
nsSplashScreenOS2::LoadBitmap() {
#ifdef XP_OS2
     HPS hps = WinGetPS( mDlg );
     mBitmap = GpiLoadBitmap (hps, NULL, IDB_SPLASH, 0L, 0L);
     WinReleasePS( hps );
#else
    // Check for '<program-name>.bmp" in same directory as executable.
    char fileName[ _MAX_PATH ];
    int fileNameLen = ::GetModuleFileName( NULL, fileName, sizeof fileName );
    if ( fileNameLen >= 3 ) {
        fileName[ fileNameLen - 3 ] = 0;
        strcat( fileName, "bmp" );
        // Try to load bitmap from that file.
        HBITMAP bitmap = (HBITMAP)::LoadImage( NULL,
                                               fileName,
                                               IMAGE_BITMAP,
                                               0,
                                               0,
                                               LR_LOADFROMFILE );
        if ( bitmap ) {
            HWND bitmapControl = GetDlgItem( mDlg, IDB_SPLASH );
            if ( bitmapControl ) {
                HBITMAP old = (HBITMAP)SendMessage( bitmapControl,
                                                    STM_SETIMAGE,
                                                    IMAGE_BITMAP,
                                                    (LPARAM)bitmap );
                // Remember bitmap so we can delete it later.
                mBitmap = bitmap;
                // Delete old bitmap.
                if ( old ) {
                    BOOL ok = DeleteObject( old );
                }
            } else {
                // Delete bitmap since it isn't going to be used.
                DeleteObject( bitmap );
            }
        }
    }
#endif
}

MRESULT EXPENTRY DialogProc( HWND dlg, ULONG msg, MPARAM mp1, MPARAM mp2 ) {
    if ( msg == WM_INITDLG ) {
        // Store dialog handle.
        nsSplashScreenOS2 *splashScreen = (nsSplashScreenOS2*)mp2;
        if ( mp2 ) {
            splashScreen->SetDialog( dlg );

            // Try to load customized bitmap.
            splashScreen->LoadBitmap();
        }

        /* Size and center the splash screen correctly. The flags in the 
         * dialog template do not do the right thing if the user's 
         * machine is using large fonts.
         */ 
        HBITMAP hbitmap = splashScreen->mBitmap;
        if ( hbitmap ) {
            BITMAPINFOHEADER bitmap;
            bitmap.cbFix = sizeof (BITMAPINFOHEADER);
            GpiQueryBitmapParameters (splashScreen->mBitmap, &bitmap);
            WinSetWindowPos( dlg,
                             HWND_TOP,
                             WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN )/2 - bitmap.cx/2,
                             WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN )/2 - bitmap.cy/2,
                             bitmap.cx,
                             bitmap.cy,
                             SWP_ACTIVATE | SWP_MOVE | SWP_SIZE );
            WinShowWindow( dlg, TRUE );
        }
        return (MRESULT)FALSE;
    }
    else if ( msg == WM_PAINT ) {
        nsSplashScreenOS2 *splashScreen = (nsSplashScreenOS2*)WinQueryWindowPtr( dlg, QWL_USER );
        HPS hps = WinBeginPaint (dlg, NULLHANDLE, NULL);
        GpiErase (hps);
        if (splashScreen->mBitmap) {
            POINTL ptl;
            ptl.x = 0;
            ptl.y = 0;
            WinDrawBitmap( hps, splashScreen->mBitmap, NULL, &ptl, CLR_NEUTRAL, CLR_BACKGROUND, DBM_NORMAL);
        }
        WinEndPaint( hps );
        return (MRESULT)TRUE;
    }
    return WinDefDlgProc (dlg, msg, mp1, mp2);
}

void nsSplashScreenOS2::SetDialog( HWND dlg ) {
    // Save dialog handle.
    mDlg = dlg;
    // Store this pointer in the dialog.
    WinSetWindowPtr( mDlg, QWL_USER, this );
}

nsSplashScreenOS2 *nsSplashScreenOS2::GetPointer( HWND dlg ) {
    // Get result from dialog user data.
    PVOID data = WinQueryWindowPtr( dlg, QWL_USER );
    return (nsSplashScreenOS2*)data;
}

void _Optlink ThreadProc(void *splashScreen) {
    HAB hab = WinInitialize( 0 );
    HMQ hmq = WinCreateMsgQueue( hab, 0 );
    WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, (PFNWP)DialogProc, NULLHANDLE, IDD_SPLASH, (MPARAM)splashScreen );
    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );
//    _endthread();
}

#ifndef XP_OS2
void
nsNativeAppSupportOS2::CheckConsole() {
    for ( int i = 1; i < __argc; i++ ) {
        if ( strcmp( "-console", __argv[i] ) == 0
             ||
             strcmp( "/console", __argv[i] ) == 0 ) {
            // Users wants to make sure we have a console.
            // Try to allocate one.
            BOOL rc = ::AllocConsole();
            if ( rc ) {
                // Console allocated.  Fix it up so that output works in
                // all cases.  See http://support.microsoft.com/support/kb/articles/q105/3/05.asp.

                // stdout
                int hCrt = ::_open_osfhandle( (long)GetStdHandle( STD_OUTPUT_HANDLE ),
                                            _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = ::_fdopen( hCrt, "w" );
                    if ( hf ) {
                        *stdout = *hf;
                        ::fprintf( stdout, "stdout directed to dynamic console\n" );
                    }
                }

                // stderr
                hCrt = ::_open_osfhandle( (long)::GetStdHandle( STD_ERROR_HANDLE ),
                                          _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = ::_fdopen( hCrt, "w" );
                    if ( hf ) {
                        *stderr = *hf;
                        ::fprintf( stderr, "stderr directed to dynamic console\n" );
                    }
                }

                // stdin?
                /* Don't bother for now.
                hCrt = ::_open_osfhandle( (long)::GetStdHandle( STD_INPUT_HANDLE ),
                                          _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = ::_fdopen( hCrt, "r" );
                    if ( hf ) {
                        *stdin = *hf;
                    }
                }
                */
            } else {
                // Failed.  Probably because there already is one.
                // There's little we can do, in any case.
            }
            // Don't bother doing this more than once.
            break;
        }
    }
    return;
}
#endif


// Create and return an instance of class nsNativeAppSupportOS2.
nsresult
NS_CreateNativeAppSupport( nsINativeAppSupport **aResult ) {
    if ( aResult ) {
        *aResult = new nsNativeAppSupportOS2;
        if ( *aResult ) {
            NS_ADDREF( *aResult );
        } else {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        return NS_ERROR_NULL_POINTER;
    }

#ifndef XP_OS2
    // Check for dynamic console creation request.
    nsNativeAppSupportOS2::CheckConsole();
#endif
    return NS_OK;
}

// Create instance of OS/2 splash screen object.
nsresult
NS_CreateSplashScreen( nsISplashScreen **aResult ) {
    if ( aResult ) {
        *aResult = 0;
#ifndef XP_OS2
        for ( int i = 1; i < __argc; i++ ) {
            if ( strcmp( "-quiet", __argv[i] ) == 0
                 ||
                 strcmp( "/quiet", __argv[i] ) == 0 ) {
                // No splash screen, please.
                return NS_OK;
            }
        }
#endif
        *aResult = new nsSplashScreenOS2;
        if ( *aResult ) {
            NS_ADDREF( *aResult );
        } else {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        return NS_ERROR_NULL_POINTER;
    }

    return NS_OK;
}

#ifndef XP_OS2
// Constants
#define MOZ_DDE_APPLICATION   "Mozilla"
#define MOZ_DDE_TOPIC         "WWW_OpenURL"
#define MOZ_DDE_MUTEX_NAME    "MozillaDDEMutex"
#define MOZ_DDE_START_TIMEOUT 30000
#define MOZ_DDE_STOP_TIMEOUT  15000
#define MOZ_DDE_EXEC_TIMEOUT  15000

// Static member definitions.
int   nsNativeAppSupportOS2::mConversations = 0;
HSZ   nsNativeAppSupportOS2::mApplication   = 0;
HSZ   nsNativeAppSupportOS2::mTopic         = 0;
DWORD nsNativeAppSupportOS2::mInstance      = 0;

// Try to initiate DDE conversation.  If that succeeds, pass
// request to server process.  Otherwise, register application
// and topic (i.e., become server process).
NS_IMETHODIMP
nsNativeAppSupportOS2::Start( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance == 0, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_ERROR_FAILURE;
    *aResult = PR_FALSE;

    // Grab mutex before doing DdeInitialize!  This is
    // important (see comment above).
	int retval;
	UINT id = ID_DDE_APPLICATION_NAME;
	char nameBuf[ 128 ];
	retval = LoadString( (HINSTANCE) NULL, id, (LPTSTR) nameBuf, sizeof(nameBuf) );
	if ( retval != 0 ) {   
		Mutex ddeLock = Mutex( MOZ_DDE_MUTEX_NAME );
		if ( ddeLock.Lock( MOZ_DDE_START_TIMEOUT ) ) {
			// Initialize DDE.
			UINT rc = DdeInitialize( &mInstance,
                                 nsNativeAppSupportOS2::HandleDDENotification,
                                 APPCLASS_STANDARD,
                                 0 );
			if ( rc == DMLERR_NO_ERROR ) {
				mApplication = DdeCreateStringHandle( mInstance,
                                                  nameBuf,
                                                  CP_WINANSI );
				mTopic       = DdeCreateStringHandle( mInstance,
                                                  MOZ_DDE_TOPIC,
                                                  CP_WINANSI );
				if ( mApplication && mTopic ) {
					// Everything OK so far, try to connect to previusly
					// started Mozilla.
					HCONV hconv = DdeConnect( mInstance, mApplication, mTopic, 0 );
                
					if ( hconv ) {
						// We're the client...
						// Get command line to pass to server.
						LPTSTR cmd = GetCommandLine();
						#if MOZ_DEBUG_DDE
						printf( "Acting as DDE client, cmd=%s\n", cmd );
						#endif
						rc = (UINT)DdeClientTransaction( (LPBYTE)cmd,
                                                     strlen( cmd ) + 1,
                                                     hconv,
                                                     0,
                                                     0,
                                                     XTYP_EXECUTE,
                                                     MOZ_DDE_EXEC_TIMEOUT,
                                                     0 );
						if ( rc ) {
							// Inform caller that request was issued.
							rv = NS_OK;
						} else {
							// Something went wrong.  Not much we can do, though...
							#if MOZ_DEBUG_DDE
							printf( "DdeClientTransaction failed, error = 0x%08X\n",
                                (int)DdeGetLastError( mInstance ) );
							#endif
						}
					} else {
						// We're going to be the server...
						#if MOZ_DEBUG_DDE
						printf( "Setting up DDE server...\n" );
						#endif

						// Next step is to register a DDE service.
						rc = (UINT)DdeNameService( mInstance, mApplication, 0, DNS_REGISTER );

						if ( rc ) {
							#if MOZ_DEBUG_DDE
							printf( "...DDE server started\n" );
							#endif
							// Tell app to do its thing.
							*aResult = PR_TRUE;
							rv = NS_OK;
						} else {
							#if MOZ_DEBUG_DDE
							printf( "DdeNameService failed, error = 0x%08X\n",
                                (int)DdeGetLastError( mInstance ) );
							#endif
						}
					}
				} else {
					#if MOZ_DEBUG_DDE
					printf( "DdeCreateStringHandle failed, error = 0x%08X\n",
                        (int)DdeGetLastError( mInstance ) );
					#endif
				}
			} else {
				#if MOZ_DEBUG_DDE
				printf( "DdeInitialize failed, error = 0x%08X\n", (int)rc );
				#endif
			}

			// Release mutex.
			ddeLock.Unlock();
		}
	}

    // Clean up.  The only case in which we need to preserve DDE stuff
    // is if we're going to be acting as server.
    if ( !*aResult ) {
        Quit();
    }

    return rv;
}

// If no DDE conversations are pending, terminate DDE.
NS_IMETHODIMP
nsNativeAppSupportOS2::Stop( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_OK;
    *aResult = PR_TRUE;

    Mutex ddeLock( MOZ_DDE_MUTEX_NAME );

    if ( ddeLock.Lock( MOZ_DDE_STOP_TIMEOUT ) ) {
        if ( mConversations == 0 ) {
            this->Quit();
        } else {
            *aResult = PR_FALSE;
        }

        ddeLock.Unlock();
    }

    return rv;
}

// Terminate DDE regardless.
NS_IMETHODIMP
nsNativeAppSupportOS2::Quit() {
    if ( mInstance ) {
        // Clean up strings.
        if ( mApplication ) {
            DdeFreeStringHandle( mInstance, mApplication );
            mApplication = 0;
        }
        if ( mTopic ) {
            DdeFreeStringHandle( mInstance, mTopic );
            mTopic = 0;
        }
        DdeUninitialize( mInstance );
        mInstance = 0;
    }


    return NS_OK;
}
#endif

PRBool NS_CanRun()
{
      return PR_TRUE;
}

#ifndef XP_OS2
#if MOZ_DEBUG_DDE
// Macro to generate case statement for a given XTYP value.
#define XTYP_CASE(t) \
    case t: result = #t; break

static nsCString uTypeDesc( UINT uType ) {
    nsCString result;
    switch ( uType ) {
    XTYP_CASE(XTYP_ADVSTART);
    XTYP_CASE(XTYP_CONNECT);
    XTYP_CASE(XTYP_ADVREQ);
    XTYP_CASE(XTYP_REQUEST);
    XTYP_CASE(XTYP_WILDCONNECT);
    XTYP_CASE(XTYP_ADVDATA);
    XTYP_CASE(XTYP_EXECUTE);
    XTYP_CASE(XTYP_POKE);
    XTYP_CASE(XTYP_ADVSTOP);
    XTYP_CASE(XTYP_CONNECT_CONFIRM);
    XTYP_CASE(XTYP_DISCONNECT);
    XTYP_CASE(XTYP_ERROR);
    XTYP_CASE(XTYP_MONITOR);
    XTYP_CASE(XTYP_REGISTER);
    XTYP_CASE(XTYP_XACT_COMPLETE);
    XTYP_CASE(XTYP_UNREGISTER);
    default: result = "XTYP_?????";
    }
    return result;
}

static nsCString hszValue( DWORD instance, HSZ hsz ) {
    // Extract string from HSZ.
    nsCString result("[");
    DWORD len = DdeQueryString( instance, hsz, NULL, NULL, CP_WINANSI );
    if ( len ) {
        char buffer[ 256 ];
        DdeQueryString( instance, hsz, buffer, sizeof buffer, CP_WINANSI );
        result += buffer;
    }
    result += "]";
    return result;
}
#else
// These are purely a safety measure to avoid the infamous "won't
// build non-debug" type Tinderbox flames.
static nsCString uTypeDesc( UINT ) {
    return nsCString( "?" );
}
static nsCString hszValue( DWORD, HSZ ) {
    return nsCString( "?" );
}
#endif


HDDEDATA CALLBACK
nsNativeAppSupportOS2::HandleDDENotification( UINT uType,       // transaction type
                                              UINT uFmt,        // clipboard data format
                                              HCONV hconv,      // handle to the conversation
                                              HSZ hsz1,         // handle to a string
                                              HSZ hsz2,         // handle to a string
                                              HDDEDATA hdata,   // handle to a global memory object
                                              ULONG dwData1,    // transaction-specific data
                                              ULONG dwData2 ) { // transaction-specific data

    #if MOZ_DEBUG_DDE
    printf( "DDE: uType  =%s\n",      uTypeDesc( uType ).GetBuffer() );
    printf( "     uFmt   =%u\n",      (unsigned)uFmt );
    printf( "     hconv  =%08x\n",    (int)hconv );
    printf( "     hsz1   =%08x:%s\n", (int)hsz1, hszValue( mInstance, hsz1 ).GetBuffer() );
    printf( "     hsz2   =%08x:%s\n", (int)hsz2, hszValue( mInstance, hsz2 ).GetBuffer() );
    printf( "     hdata  =%08x\n",    (int)hdata );
    printf( "     dwData1=%08x\n",    (int)dwData1 );
    printf( "     dwData2=%08x\n",    (int)dwData2 );
    #endif

    HDDEDATA result = 0;
    if ( uType & XCLASS_BOOL ) {
        switch ( uType ) {
        case XTYP_CONNECT:
        case XTYP_CONNECT_CONFIRM:
            // Make sure its for our service/topic.
            if ( DdeCmpStringHandles( hsz1, mTopic ) == 0
                 &&
                 DdeCmpStringHandles( hsz2, mApplication ) == 0 ) {
                // We support this connection.
                result = (HDDEDATA)1;
            }
        }
    } else if ( uType & XCLASS_DATA ) {
    } else if ( uType & XCLASS_FLAGS ) {
        if ( uType == XTYP_EXECUTE ) {
            // Prove that we received the request.
            DWORD bytes;
            LPBYTE request = DdeAccessData( hdata, &bytes );
            #if MOZ_DEBUG_DDE
            printf( "Handling dde request: [%s]...\n", (char*)request );
            #endif
            HandleRequest( request );
            result = (HDDEDATA)DDE_FACK;
        } else {
            result = (HDDEDATA)DDE_FNOTPROCESSED;
        }
    } else if ( uType & XCLASS_NOTIFICATION ) {
    }
    return result;
}

// Handle DDE request.  The argument is the command line received by the
// DDE client process.  We convert that string to an nsICmdLineService
// object via GetCmdLineArgs.  Then, we look for certain well-known cmd
// arguments.  This replicates code elsewhere, to some extent,
// unfortunately (if you can fix that, please do).
void
nsNativeAppSupportOS2::HandleRequest( LPBYTE request ) {
    // Parse command line.
    nsCOMPtr<nsICmdLineService> args;
    nsresult rv = GetCmdLineArgs( request, getter_AddRefs( args ) );
    if ( NS_SUCCEEDED( rv ) ) {
        nsXPIDLCString arg;
        if (NS_SUCCEEDED(args->GetURLToLoad(getter_Copies(arg) ) ) &&
            (const char*)arg ) {
            // Launch browser.
            #if MOZ_DEBUG_DDE
            printf( "Launching browser on url [%s]...\n", (const char*)arg );
            #endif
            (void)OpenWindow( "chrome://navigator/content/", arg );
        }
        else if (NS_SUCCEEDED(args->GetCmdLineValue("-chrome", getter_Copies(arg))) &&
                 (const char*)arg ) {
            // Launch chrome.
            #if MOZ_DEBUG_DDE
            printf( "Launching chrome url [%s]...\n", (const char*)arg );
            #endif
            (void)OpenWindow( arg, "" );
        }
        else if (NS_SUCCEEDED(args->GetCmdLineValue("-edit", getter_Copies(arg))) &&
                 (const char*)arg ) {
            // Launch composer.
            #if MOZ_DEBUG_DDE
            printf( "Launching editor on url [%s]...\n", arg );
            #endif
            (void)OpenWindow( "chrome://editor/content/", arg );
        } else if ( NS_SUCCEEDED( args->GetCmdLineValue( "-mail", getter_Copies(arg))) &&
                    (const char*)arg ) {
            // Launch composer.
            #if MOZ_DEBUG_DDE
            printf( "Launching mail...\n" );
            #endif
            (void)OpenWindow( "chrome://messenger/content/", "" );
        } else {
            #if MOZ_DEBUG_DDE
            printf( "Unknown request [%s]\n", (char*) request );
            #endif
        }
    
    }
}

// Parse command line args according to MS spec
// (see "Parsing C++ Command-Line Arguments" at
// http://msdn.microsoft.com/library/devprods/vs6/visualc/vclang/_pluslang_parsing_c.2b2b_.command.2d.line_arguments.htm).
nsresult
nsNativeAppSupportOS2::GetCmdLineArgs( LPBYTE request, nsICmdLineService **aResult ) {
    nsresult rv = NS_OK;

    int justCounting = 1;
    char **argv = 0;
    // Flags, etc.
    int init = 1;
    int between, quoted, bSlashCount;
    int argc;
    char *p;
    nsCString arg;
    // We loop if we've not finished the second pass through.
    while ( 1 ) {
        // Initialize if required.
        if ( init ) {
            p = (char*)request;
            between = 1;
            argc = quoted = bSlashCount = 0;

            init = 0;
        }
        if ( between ) {
            // We are traversing whitespace between args.
            // Check for start of next arg.
            if (  *p != 0 && !isspace( *p ) ) {
                // Start of another arg.
                between = 0;
                arg = "";
                switch ( *p ) {
                    case '\\':
                        // Count the backslash.
                        bSlashCount = 1;
                        break;
                    case '"':
                        // Remember we're inside quotes.
                        quoted = 1;
                        break;
                    default:
                        // Add character to arg.
                        arg += *p;
                        break;
                }
            } else {
                // Another space between args, ignore it.
            }
        } else {
            // We are processing the contents of an argument.
            // Check for whitespace or end.
            if ( *p == 0 || ( !quoted && isspace( *p ) ) ) {
                // Process pending backslashes (interpret them 
                // literally since they're not followed by a ").
                while( bSlashCount ) {
                    arg += '\\';
                    bSlashCount--;
                }
                // End current arg.
                if ( !justCounting ) {
                    argv[argc] = new char[ arg.Length() + 1 ];
                    strcpy( argv[argc], arg.GetBuffer() ); 
                }
                argc++;
                // We're now between args.
                between = 1;
            } else {
                // Still inside argument, process the character.
                switch ( *p ) {
                    case '"':
                        // First, digest preceding backslashes (if any).
                        while ( bSlashCount > 1 ) {
                            // Put one backsplash in arg for each pair.
                            arg += '\\';
                            bSlashCount -= 2;
                        }
                        if ( bSlashCount ) {
                            // Quote is literal.
                            arg += '"';
                            bSlashCount = 0;
                        } else {
                            // Quote starts or ends a quoted section.
                            if ( quoted ) {
                                // Check for special case of consecutive double
                                // quotes inside a quoted section.
                                if ( *(p+1) == '"' ) {
                                    // This implies a literal double-quote.  Fake that
                                    // out by causing next double-quote to look as
                                    // if it was preceded by a backslash.
                                    bSlashCount = 1;
                                } else {
                                    quoted = 0;
                                }
                            } else {
                                quoted = 1;
                            }
                        }
                        break;
                    case '\\':
                        // Add to count.
                        bSlashCount++;
                        break;
                    default:
                        // Accept any preceding backslashes literally.
                        while ( bSlashCount ) {
                            arg += '\\';
                            bSlashCount--;
                        }
                        // Just add next char to the current arg.
                        arg += *p;
                        break;
                }
            }
        }
        // Check for end of input.
        if ( *p ) {
            // Go to next character.
            p++;
        } else {
            // If on first pass, go on to second.
            if ( justCounting ) {
                // Allocate argv array.
                argv = new char*[ argc ];
    
                // Start second pass
                justCounting = 0;
                init = 1;
            } else {
                // Quit.
                break;
            }
        }
    }

    // OK, now create nsICmdLineService object from argc/argv.
    static NS_DEFINE_CID( kCmdLineServiceCID,    NS_COMMANDLINE_SERVICE_CID );
    rv = nsComponentManager::CreateInstance( kCmdLineServiceCID,
                                             0,
                                             NS_GET_IID( nsICmdLineService ),
                                             (void**)aResult );
    if ( NS_FAILED( rv ) || NS_FAILED( ( rv = (*aResult)->Initialize( argc, argv ) ) ) ) {
        #if MOZ_DEBUG_DDE
        printf( "Error creating command line service = 0x%08X\n", (int)rv );
        #endif
    }

    // Cleanup.
    while ( argc ) {
        delete [] argv[ --argc ];
    }
    delete [] argv;

    return rv;
}

nsresult
nsNativeAppSupportOS2::OpenWindow( const char*urlstr, const char *args ) {
    nsresult rv;
    static NS_DEFINE_CID( kAppShellServiceCID,    NS_APPSHELL_SERVICE_CID );
    NS_WITH_SERVICE(nsIAppShellService, appShellService, kAppShellServiceCID, &rv)
    if ( NS_SUCCEEDED( rv ) ) {
        nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;
        JSContext *jsContext;
        rv = appShellService->GetHiddenWindowAndJSContext( getter_AddRefs( hiddenWindow ),
                                                           &jsContext );
        if ( NS_SUCCEEDED( rv ) ) {
            void *stackPtr;
            jsval *argv = JS_PushArguments( jsContext,
                                            &stackPtr,
                                            "ssss",
                                            urlstr,
                                            "_blank",
                                            "chrome,dialog=no,all",
                                            args );
            if( argv ) {
                nsCOMPtr<nsIDOMWindowInternal> newWindow;
                rv = hiddenWindow->OpenDialog( jsContext,
                                               argv,
                                               4,
                                               getter_AddRefs( newWindow ) );
                JS_PopArguments( jsContext, stackPtr );
            }
        } else {
            #ifdef MOZ_DEBUG_DDE
            printf( "GetHiddenWindowAndJSContext failed, rv=0x%08X\n", (int)rv );
            #endif
        }
    }
    return rv;
}
#endif
