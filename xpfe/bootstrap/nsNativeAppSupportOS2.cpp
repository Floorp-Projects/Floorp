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

// For server mode systray icon.
#include "nsIStringBundle.h"

#include "nsNativeAppSupportBase.h"
#include "nsString.h"
#include "nsICmdLineService.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsICmdLineHandler.h"
#include "nsIDOMWindow.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsArray.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"         
#include "nsIBaseWindow.h"       
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsIProfileInternal.h"
#include "rdf.h"
#include "nsIXULWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPref.h"
#include "nsIPromptService.h"
#include "nsNetCID.h"
#include "nsIHttpProtocolHandler.h"

// These are needed to load a URL in a browser window.
#include "nsIDOMLocation.h"
#include "nsIJSContextStack.h"
#include "nsIWindowMediator.h"

#define INCL_PM
#define INCL_GPI
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

#include "nsNativeAppSupportOS2.h"

#define TURBO_QUIT 1


// getting from nsAppRunner.  Use to help track down arguments.
extern char ** __argv;
extern int    __argc;


typedef ULONG DWORD;
typedef PBYTE LPBYTE;
typedef HDATA HDDEDATA;
#define CP_WINANSI    0  //  When 0 is specified for codepage on these
                         //  dde functions, it will use the codepage
                         //  that is associated with the current thread.
                         //  CP_WINANSI in win32 means that the non
                         //  unicode version of DdeCreateStringHandle
                         //  was used.

// Adding to support DDE
#define ID_MESSAGEWINDOW        0xDEAF
/* trying to keep this like Window's COPYDATASTRUCT, but a compiler error is
 * forcing me to add chBuff so that we can append the data to the end of the
 * structure
 */
typedef struct _COPYDATASTRUCT
{
   ULONG   dwData;
   ULONG   cbData;
   PVOID   lpData;
   CHAR    chBuff;
}COPYDATASTRUCT, *PCOPYDATASTRUCT;
#define WM_COPYDATA             (WM_USER + 60)

char szCommandLine[2*CCHMAXPATH];


static HWND hwndForDOMWindow( nsISupports * );

static
nsresult
GetMostRecentWindow(const PRUnichar* aType, nsIDOMWindowInternal** aWindow) {
    nsresult rv;
    nsCOMPtr<nsIWindowMediator> med( do_GetService( NS_RDF_DATASOURCE_CONTRACTID_PREFIX "window-mediator", &rv ) );
    if ( NS_FAILED( rv ) )
        return rv;

    if ( med )
        return med->GetMostRecentWindow( aType, aWindow );

    return NS_ERROR_FAILURE;
}

static
void
activateWindow( nsIDOMWindowInternal *win ) {
    // Try to get native window handle.
    HWND hwnd = hwndForDOMWindow( win );
    if ( hwnd ) {

        /* if we are looking at a client window, then we really probably want
         * the frame so that we can manipulate THAT
         */
        LONG id = WinQueryWindowUShort( hwnd, QWS_ID );
        if( id == FID_CLIENT )
        {
           hwnd = WinQueryWindow( hwnd, QW_PARENT );
        }

        // Restore the window in case it is minimized.
        // Use the OS call, if possible.
        WinSetWindowPos( hwnd, 0L, 0L, 0L, 0L, 0L, 
                         SWP_SHOW | SWP_RESTORE | SWP_ACTIVATE );
    } else {
        // Use internal method.
        win->Focus();
    }
}



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

// Simple Win32 mutex wrapper.
struct Mutex {
    Mutex( const char *name )
        : mName( name ),
          mHandle( 0 ),
          mState( -1 ) {
        DosCreateMutexSem( mName.get(), &mHandle, 0, FALSE );
#if MOZ_DEBUG_DDE
        printf( "CreateMutex error = 0x%08X\n", (int)GetLastError() );
#endif
    }
    ~Mutex() {
        if ( mHandle ) {
            // Make sure we release it if we own it.
            Unlock();

            APIRET rc = DosCloseMutexSem( mHandle );
#if MOZ_DEBUG_DDE
            if ( rc != NO_ERROR ) {
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
            mState = DosRequestMutexSem( mHandle, timeout );
#if MOZ_DEBUG_DDE
            printf( "...wait complete, result = 0x%08X\n", (int)mState );
#endif
            return mState == NO_ERROR;
        } else {
            return FALSE;
        }
    }
    void Unlock() {
        if ( mHandle && mState == NO_ERROR ) {
#if MOZ_DEBUG_DDE
            printf( "Releasing DDE mutex\n" );
#endif
            DosReleaseMutexSem( mHandle );
            mState = -1;
        }
    }
private:
    nsCString mName;
    HMTX      mHandle;
    DWORD     mState;
};

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

/* Update 2001 March
 *
 * A significant DDE bug in Windows is causing Mozilla to get wedged at
 * startup.  This is detailed in Bugzill bug 53952
 * (http://bugzilla.mozilla.org/show_bug.cgi?id=53952).
 *
 * To resolve this, we are using a new strategy:
 *   o Use a "message window" to detect that Mozilla is already running and
 *     to pass requests from a second instance back to the first;
 *   o Run only as a "DDE server" (not as DDE client); this avoids the
 *     problematic call to DDEConnect().
 *
 * We still use the mutex semaphore to protect the code that detects
 * whether Mozilla is already running.
 */

class nsNativeAppSupportOS2 : public nsNativeAppSupportBase {
public:
    // Overrides of base implementation.
    NS_IMETHOD Start( PRBool *aResult );
    NS_IMETHOD Stop( PRBool *aResult );
    NS_IMETHOD Quit();
    NS_IMETHOD StartServerMode();
    NS_IMETHOD OnLastWindowClosing( nsIXULWindow *aWindow );
    NS_IMETHOD SetIsServerMode( PRBool isServerMode );
    NS_IMETHOD EnsureProfile(nsICmdLineService* args);


    // The "old" Start method (renamed).
    NS_IMETHOD StartDDE();

    // Utility function to handle a Win32-specific command line
    // option: "-console", which dynamically creates a Windows
    // console.
    void CheckConsole();

private:
    static HDDEDATA APIENTRY HandleDDENotification( ULONG    idInst,
                                                 USHORT   uType,
                                                 USHORT   uFmt,
                                                 HCONV    hconv,
                                                 HSZ      hsz1,
                                                 HSZ      hsz2,
                                                 HDDEDATA hdata,
                                                 ULONG    dwData1,
                                                 ULONG    dwData2 );
    static void HandleRequest( LPBYTE request, PRBool newWindow = PR_TRUE );
    static nsCString ParseDDEArg( HSZ args, int index );
    static void ActivateLastWindow();
    static HDDEDATA    CreateDDEData( DWORD value );
    static PRBool   InitTopicStrings();
    static int      FindTopic( HSZ topic );
    static nsresult GetCmdLineArgs( LPBYTE request, nsICmdLineService **aResult );
    static nsresult OpenWindow( const char *urlstr, const char *args );
    static nsresult OpenBrowserWindow( const char *args, PRBool newWindow = PR_TRUE );
    static nsresult ReParent( nsISupports *window, HWND newParent );
    static nsresult GetStartupURL(nsICmdLineService *args, nsCString& taskURL);
    static void     SetupSysTrayIcon();
    static void     RemoveSysTrayIcon();


    static int   mConversations;
    enum {
        topicOpenURL,
        topicActivate,
        topicCancelProgress,
        topicVersion,
        topicRegisterViewer,
        topicUnRegisterViewer,
        // Note: Insert new values above this line!!!!!
        topicCount // Count of the number of real topics
    };
#ifdef DOWENEED
    static NOTIFYICONDATA mIconData;
    static HMENU          mTrayIconMenu;
#endif /* DOWENEED */

    static HSZ   mApplication, mTopics[ topicCount ];
    static DWORD mInstance;
    static char *mAppName;
    static nsIDOMWindow *mInitialWindow;
    friend struct MessageWindow;
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
#ifdef DOWENEED
        // Fix for bugs:
        //  http://bugzilla.mozilla.org/show_bug.cgi?id=26581
        //  http://bugzilla.mozilla.org/show_bug.cgi?id=65974
        //  http://bugzilla.mozilla.org/show_bug.cgi?id=29172
        //  http://bugzilla.mozilla.org/show_bug.cgi?id=45805
        // As the splash-screen is in a seperate thread, Windows considers
        // this the "foreground" thread.  When our main windows on the main
        // thread are activated, they are treated like windows from a different
        // application, so Windows 2000 and 98 both leave the window in the background.
        // Therefore, we post a message to the splash-screen thread that includes
        // the hwnd of the window we want moved to the foreground.  This thread
        // can then successfully bring the top-level window to the foreground.
        nsCOMPtr<nsIDOMWindowInternal> topLevel;
        GetMostRecentWindow(nsnull, getter_AddRefs( topLevel ) );
        HWND hWndTopLevel = topLevel ? hwndForDOMWindow(topLevel) : 0;
#endif /* DOWENEED */

        // Dismiss the dialog.
        WinPostMsg(mDlg, WM_CLOSE, 0, 0);
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
#ifdef DOWENEED
    else if (msg == WM_CLOSE) {
        // Before killing ourself, set the top-level current.
        // See comments in nsSplashScreenWin::Hide() above.
        HWND topLevel = (HWND)mp2;
        if (topLevel)
            ::SetForegroundWindow(topLevel);
        // Destroy the dialog
        ::EndDialog(dlg, 0);
        // Release custom bitmap (if there is one).
        HBITMAP bitmap = (HBITMAP)wp;
        if ( bitmap ) {
            ::DeleteObject( bitmap );
        }
    }
#endif /* DOWENEED */
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

void
nsNativeAppSupportOS2::CheckConsole() {
    for ( int i = 1; i < __argc; i++ ) {
        if ( strcmp( "-console", __argv[i] ) == 0
             ||
             strcmp( "/console", __argv[i] ) == 0 ) {
#ifdef DOWENEED
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
#endif /* DOWENEED */
        } else if ( strcmp( "-turbo", __argv[i] ) == 0
                    ||
                    strcmp( "/turbo", __argv[i] ) == 0
                    ||
                    strcmp( "-server", __argv[i] ) == 0                                              
                    ||
                    strcmp( "/server", __argv[i] ) == 0 ) {         
            // Start in server mode (and suppress splash screen).   
            SetIsServerMode( PR_TRUE );                             
            __argv[i] = "-nosplash"; // Bit of a hack, but it works!
            // Ignore other args.                                   
            break;                                                  
        }
    }
    return;
}


// Create and return an instance of class nsNativeAppSupportOS2.
nsresult
NS_CreateNativeAppSupport( nsINativeAppSupport **aResult ) {
    if ( aResult ) {
        nsNativeAppSupportOS2 *pNative = new nsNativeAppSupportOS2;
        if ( pNative ) {                                           
            *aResult = pNative;                                    
            NS_ADDREF( *aResult );                                 
            // Check for dynamic console creation request.         
            pNative->CheckConsole();                               
        } else {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        return NS_ERROR_NULL_POINTER;
    }

    return NS_OK;
}

// Create instance of OS/2 splash screen object.
nsresult
NS_CreateSplashScreen( nsISplashScreen **aResult ) {
    if ( aResult ) {
        *aResult = 0;
        CHAR pBuffer[3];
        PrfQueryProfileString( HINI_USERPROFILE, "PM_ControlPanel", "LogoDisplayTime", "1", pBuffer, 3);
        if (pBuffer[0] == '0') {
          return NS_OK;
        } /* endif */
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

// Constants
#define MOZ_DDE_APPLICATION    "Mozilla"
#ifdef XP_OS2
/* os/2 named semaphores must begin with "\\SEM32\\" to be valid */
#define MOZ_MUTEX_PREPEND      "\\SEM32\\"
#endif /* XP_OS2 */
#define MOZ_STARTUP_MUTEX_NAME "StartupMutex"
#define MOZ_DDE_START_TIMEOUT 30000
#define MOZ_DDE_STOP_TIMEOUT  15000
#define MOZ_DDE_EXEC_TIMEOUT  15000

// The array entries must match the enum ordering!
const char * const topicNames[] = { "WWW_OpenURL",
                                    "WWW_Activate",
                                    "WWW_CancelProgress",
                                    "WWW_Version",
                                    "WWW_RegisterViewer",
                                    "WWW_UnRegisterViewer" };

// Static member definitions.
int   nsNativeAppSupportOS2::mConversations = 0;
HSZ   nsNativeAppSupportOS2::mApplication   = 0;
HSZ   nsNativeAppSupportOS2::mTopics[nsNativeAppSupportOS2::topicCount] = { 0 };
DWORD nsNativeAppSupportOS2::mInstance      = 0;
nsIDOMWindow* nsNativeAppSupportOS2::mInitialWindow = nsnull;


// Added this as pmddeml has no api like this
int DdeCmpStringHandles( HSZ hsz1, HSZ hsz2 )
{
  char chhsz1[CCHMAXPATH], chhsz2[CCHMAXPATH];
  int rc = -1;

  /* I am assuming that the strings will never be more that CCHMAXPATH in
   * length.  Safe bet (for now).  To find true length, call WinDdeQueryString
   * and pass in (hsz, NULL, 0L, 0) and it will return iLength.  Passing 0
   * for codepage will use the codepage of the current thread.
   */
  WinDdeQueryString( hsz1, chhsz1, sizeof( chhsz1 ), 0 );
  WinDdeQueryString( hsz2, chhsz2, sizeof( chhsz2 ),0 );

  rc = stricmp( chhsz1, chhsz2 );

  return(rc);
}


char *GetCommandLine()
{
   /* This function is meant to be like the Window's GetCommandLine() function.
    * The value returned by pPIB->pib_pchcmd is of the format:
    * <executable>\0<command line parameters>.  So the first string is the
    * .exe and the second string is what followed on the command line.  Dorky,
    * but I guess it'll have to do.
    */
   PTIB pTIB = NULL;
   PPIB pPIB = NULL;
   APIRET rc = NO_ERROR;
   char *pchParam = NULL;

   rc = DosGetInfoBlocks( &pTIB, &pPIB );
   if( rc == NO_ERROR )
   {
      INT iLen = 0;
      char *pchTemp = NULL;
      pchParam = pPIB->pib_pchcmd;
      strcpy( szCommandLine, pchParam );
      iLen = strlen( pchParam );

      /* szCommandLine[iLen] is '\0', so see what's next.  Probably be another
       * '\0', a space or start of the arguments
       */
      pchTemp = &(pchParam[iLen+1]);

      /* At this point, szCommandLine holds just the program name.  Now we
       * go for the parameters.  If our next value is a space, ignore it
       * and go for the next character
       */
      if( *pchTemp )
      {
         szCommandLine[iLen] = ' ';
         iLen++;
         if( *pchTemp == ' ' )
         {
            pchTemp++;
         }
         strcpy( &(szCommandLine[iLen]), pchTemp );
      }

   }

   return( szCommandLine );
}

typedef struct _DDEMLFN
{
   PFN   *fn;
   ULONG ord; 
} DDEMLFN, *PDDEMLFN;

DDEMLFN ddemlfnTable[] = 
{
   { (PFN *)&WinDdeAbandonTransaction   ,100 },
   { (PFN *)&WinDdeAccessData           ,101 },
   { (PFN *)&WinDdeAddData              ,102 },
   { (PFN *)&WinDdeSubmitTransaction    ,103 },
   { (PFN *)&WinDdeCompareStringHandles ,104 },
   { (PFN *)&WinDdeConnect              ,105 },
   { (PFN *)&WinDdeConnectList          ,106 },
   { (PFN *)&WinDdeCreateDataHandle     ,107 },
   { (PFN *)&WinDdeCreateStringHandle   ,108 },
   { (PFN *)&WinDdeDisconnect           ,109 },
   { (PFN *)&WinDdeDisconnectList       ,110 },
   { (PFN *)&WinDdeEnableCallback       ,111 },
   { (PFN *)&WinDdeFreeDataHandle       ,112 },
   { (PFN *)&WinDdeFreeStringHandle     ,113 },
   { (PFN *)&WinDdeGetData              ,114 },
   { (PFN *)&WinDdeInitialize           ,116 },
   { (PFN *)&WinDdeKeepStringHandle     ,117 },
   { (PFN *)&WinDdeNameService          ,118 },
   { (PFN *)&WinDdePostAdvise           ,119 },
   { (PFN *)&WinDdeQueryConvInfo        ,120 },
   { (PFN *)&WinDdeQueryNextServer      ,121 },
   { (PFN *)&WinDdeQueryString          ,122 },
   { (PFN *)&WinDdeReconnect            ,123 },
   { (PFN *)&WinDdeSetUserHandle        ,124 },
   { (PFN *)&WinDdeUninitialize         ,126 },
   { (PFN *)NULL                           ,  0 }
};

/* SetupOS2ddeml makes sure that we can get pointers to all of the DDEML 
 * functions in demlfnTable using entrypoints.  If we can't find one of them
 * or can't load pmddeml.dll, then returns FALSE
 */
BOOL SetupOS2ddeml()
{
    BOOL bRC = FALSE;
    HMODULE hmodDDEML = NULLHANDLE;
    APIRET rc = NO_ERROR;
    ULONG ulVersion = 0;

    rc = DosLoadModule( NULL, 0, "PMDDEML", &hmodDDEML );
    if( rc == NO_ERROR )
    {
       int i=0;
       /* all of this had better work.  Get ready to be a success! */
       bRC = TRUE;
       for( i; ddemlfnTable[i].ord != 0; i++ )
       {
          rc = DosQueryProcAddr( hmodDDEML, ddemlfnTable[i].ord, NULL,
                                 ddemlfnTable[i].fn );
          if( rc != NO_ERROR )
          {
             /* we're horked.  Return rc = horked */
             bRC = FALSE;
             break;
          }
       }
    } /* load PMDDEML */

    return( bRC );
}

#ifdef DOWENEED
NOTIFYICONDATA nsNativeAppSupportOS2::mIconData = { sizeof(NOTIFYICONDATA),
                                                    0,
                                                    1, 
                                                    NIF_ICON | NIF_MESSAGE | NIF_TIP,
                                                    WM_USER,
                                                    0,
                                                    0 };
HMENU nsNativeAppSupportOS2::mTrayIconMenu = 0;
#endif /* DOWENEED */


// Message window encapsulation.
struct MessageWindow {
    // ctor/dtor are simplistic
    MessageWindow() {
        // Try to find window.
        // XXX need to improve this to use check the class
        mHandle = WinWindowFromID( HWND_OBJECT, ID_MESSAGEWINDOW ); 
    }

    // Act like an HWND.
    operator HWND() {
        return mHandle;
    }

    // Class name: appName + "MessageWindow"
    static const char *className() {
        static char classNameBuffer[128];
        static char *mClassName = 0;
        if ( !mClassName ) { 
            sprintf( classNameBuffer,
                         "%s%s",
                         nsNativeAppSupportOS2::mAppName,
                         "MessageWindow" );
            mClassName = classNameBuffer;
        }
        return mClassName;
    }

    // Create: Register class and create window.
    NS_IMETHOD Create() {

        // Register the window class.
        NS_ENSURE_TRUE( WinRegisterClass( 0, className(), 
                                          (PFNWP)&MessageWindow::WindowProc, 
                                          0L, 0 ), 
                        NS_ERROR_FAILURE );

        /* Giving a size but offscreen so that won't be seen, even if all
         * goes wrong.  Giving a size so that can be seen and understood by
         * tools
         */
        const char * pszClassName = className();
        mHandle = WinCreateWindow( HWND_OBJECT,
                                                   pszClassName,
                                                   NULL,        // name
                                                   WS_DISABLED, // style
                                                   0,0,     // x, y
                                                   0,0,       // cx,cy
                                                   HWND_DESKTOP,// owner
                                                   HWND_BOTTOM,  // hwndbehind
                                                   ID_MESSAGEWINDOW, // id
                                                   NULL,        // pCtlData
                                                   NULL );      // pres params

#if MOZ_DEBUG_DDE
        printf( "Message window = 0x%08X\n", (int)mHandle );
#endif

        return NS_OK;
    }

    // SendRequest: Pass string via WM_COPYDATA to message window.
    NS_IMETHOD SendRequest( const char *cmd ) {
#ifdef XP_OS2
        /* Nothing like WM_COPYDATA in OS/2, where the OS allows pointers to be
         * passed to a different process and automatically accessible by that
         * process.  So we have to create shared mem on our side and then the
         * process that gets the WM_COPYDATA message has to do a 
         * DosGetSharedMem on this pointer to be able to access the data
         */

        COPYDATASTRUCT *pcds;
        APIRET rc = NO_ERROR;
        PVOID pvData = NULL;
        ULONG ulSize = sizeof(COPYDATASTRUCT)+strlen(cmd)+1;
        rc = DosAllocSharedMem( &pvData, NULL, ulSize,
                                (PAG_COMMIT|PAG_READ|PAG_WRITE|OBJ_GETTABLE) );

        if( rc != NO_ERROR )
        {
           /* don't even try doing anything else.  Windows doesn't worry about
            * errors so I guess that we shouldn't either
            */
           return NS_OK;
        }

        memset( pvData, '\0', ulSize );
        pcds = (COPYDATASTRUCT *)(pvData);
        pcds->dwData = 0;
        pcds->cbData = strlen(cmd)+1;
        /* put the data in the buffer space immediately after the 
         * COPYDATASTRUCT
         */
        pcds->lpData = &(pcds->chBuff);
        if( cmd )
        {
           strcpy( (char *)pcds->lpData, cmd );
        }
        WinSendMsg( mHandle, WM_COPYDATA, 0, (MPARAM)pcds );
        DosFreeMem( pvData );
#else
        COPYDATASTRUCT cds = { 0, strlen( cmd ) + 1, (void*)cmd };
        ::SendMessage( mHandle, WM_COPYDATA, 0, (LPARAM)&cds );
#endif /* XP_OS2 */

        return NS_OK;
    }

    // Window proc.
    static MRESULT EXPENTRY WindowProc( HWND msgWindow, ULONG msg, MPARAM wp, 
                                        MPARAM lp )
    {
        MRESULT rc = (MRESULT)TRUE;

        if ( msg == WM_COPYDATA ) {
            // This is an incoming request.
            COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lp;
#ifdef XP_OS2
            DosGetSharedMem( (PVOID)cds, PAG_READ|PAG_WRITE );
#endif /* XP_OS2 */
#if MOZ_DEBUG_DDE
            printf( "Incoming request: %s\n", (const char*)cds->lpData );
#endif
            (void)nsNativeAppSupportOS2::HandleRequest( (LPBYTE)cds->lpData );
 }
#ifdef DOWENEED
 else if ( msg == WM_USER ) {
     if ( lp == WM_RBUTTONUP ) {
         // Show menu with Exit disabled/enabled appropriately.
         nsCOMPtr<nsIDOMWindowInternal> win;
         GetMostRecentWindow( 0, getter_AddRefs( win ) );
         ::EnableMenuItem( nsNativeAppSupportWin::mTrayIconMenu, 1, win ? MF_GRAYED : MF_ENABLED );
         POINT pt;
         GetCursorPos( &pt );

         SetForegroundWindow(msgWindow);
         int selectedItem = ::TrackPopupMenu( nsNativeAppSupportWin::mTrayIconMenu,
                                              TPM_NONOTIFY | TPM_RETURNCMD |
                                              TPM_RIGHTBUTTON,
                                              pt.x,
                                              pt.y,
                                              0,
                                              msgWindow,
                                              0 );

         switch (selectedItem) {
         case TURBO_QUIT:
             (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)"mozilla -kill" );
             break;
         }
         PostMessage(msgWindow, WM_NULL, 0, 0);
     } else if ( lp == WM_LBUTTONDBLCLK ) {
         // Activate window or open a new one.
         nsCOMPtr<nsIDOMWindowInternal> win;
         GetMostRecentWindow( 0, getter_AddRefs( win ) );
         if ( win ) {
             activateWindow( win );
         } else {
            // This will do the default thing and open a new window.
            (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)"mozilla" );
         }
     }
  }
#endif /* DOWENEED */

#ifdef XP_OS2
    /* We have to return a FALSE from WM_CREATE or this window will never
     * get off of the ground
     */
    else if ( msg == WM_CREATE ) {
        rc = (MRESULT)FALSE;
    }
#endif /* XP_OS2 */

    return rc;
}

private:
    HWND mHandle;
}; // struct MessageWindow

static char nameBuffer[128] = { 0 };
char *nsNativeAppSupportOS2::mAppName = nameBuffer;

/* Start: Tries to find the "message window" to determine if it
 *        exists.  If so, then Mozilla is already running.  In that
 *        case, we use the handle to the "message" window and send
 *        a request corresponding to this process's command line
 *        options.
 *        
 *        If not, then this is the first instance of Mozilla.  In
 *        that case, we create and set up the message window.
 *
 *        The checking for existance of the message window must
 *        be protected by use of a mutex semaphore.
 */
NS_IMETHODIMP
nsNativeAppSupportOS2::Start( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance == 0, NS_ERROR_NOT_INITIALIZED );

    PRBool useDDE = PR_TRUE;

    nsresult rv = NS_ERROR_FAILURE;
    *aResult = PR_FALSE;

    for ( int i = 1; i < __argc; i++ ) {
        if ( strcmp( "-nodde", __argv[i] ) == 0 ||
             strcmp( "/nodde", __argv[i] ) == 0 ) {
            useDDE = PR_FALSE;
        }
    }

    // Grab mutex first.
    int retval;
    UINT id = ID_DDE_APPLICATION_NAME;
    char nameBuf[ 128 ];
    retval = WinLoadString( NULLHANDLE, NULLHANDLE, id, sizeof(nameBuffer), nameBuffer );
    if ( retval == 0 ) {
        // No app name; just keep running.
        *aResult = PR_TRUE;
        return NS_OK;
    }

    // Build mutex name from app name.
    char mutexName[128];
    sprintf( mutexName, "%s%s%s", MOZ_MUTEX_PREPEND, nameBuffer, MOZ_STARTUP_MUTEX_NAME );
    Mutex startupLock = Mutex( mutexName );

    NS_ENSURE_TRUE( startupLock.Lock( MOZ_DDE_START_TIMEOUT ), NS_ERROR_FAILURE );

#ifdef XP_OS2
    /* We need to have a message queue to do the MessageWindow stuff (for both
     * Create() and SendRequest()).  If we don't have one, make it now.
     * If we are going to end up going away right after ::Start() returns,
     * then make sure to clean up the message queue.
     */
    MQINFO mqinfo;
    HAB    hab;
    HMQ  hmqCurrent = WinQueryQueueInfo( HMQ_CURRENT, &mqinfo, 
                                         sizeof( MQINFO ) );
    if( !hmqCurrent )
    {
        hab = WinInitialize( 0 );
        hmqCurrent = WinCreateMsgQueue( hab, 0 );
    }

#endif /* XP_OS */

    // Search for existing message window.
    MessageWindow msgWindow;
    if ( (HWND)msgWindow ) {
        // We are a client process.  Pass request to message window.
        char *cmd = GetCommandLine();
        rv = msgWindow.SendRequest( cmd );
    } else {
        // We will be server.
        rv = msgWindow.Create();
        if ( NS_SUCCEEDED( rv ) ) {
            if (useDDE) {
                // Start up DDE server.
                this->StartDDE();
            }
            // Tell caller to spin message loop.
            *aResult = PR_TRUE;
        }
    }

    startupLock.Unlock();

#ifdef XP_OS2
    if( *aResult == PR_FALSE )
    {
        /* This process isn't going to continue much longer.  Make sure that we
         * clean up the message queue
         */
        if( hmqCurrent )
        {
           WinDestroyMsgQueue( hmqCurrent );
        }
        if( hab )
        {
           WinTerminate( hab );
        }
    }
#endif /* XP_OS2 */

    return rv;
}

PRBool
nsNativeAppSupportOS2::InitTopicStrings() {
    for ( int i = 0; i < topicCount; i++ ) {
        if ( !( mTopics[ i ] = WinDdeCreateStringHandle( (PSZ)topicNames[ i ], CP_WINANSI ) ) ) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

int
nsNativeAppSupportOS2::FindTopic( HSZ topic ) {
    for ( int i = 0; i < topicCount; i++ ) {
        if ( DdeCmpStringHandles( topic, mTopics[i] ) == 0 ) {
            return i;
        }
    }
    return -1;
}

// Start DDE server.
//
// This used to be the Start() method when we were using DDE as the
// primary IPC mechanism between secondary Mozilla processes and the
// initial "server" process.
//
// Now, it simply initializes the DDE server.  The caller must check
// that this process is to be the server, and, must acquire the DDE
// startup mutex semaphore prior to calling this routine.  See ::Start(),
// above.
NS_IMETHODIMP
nsNativeAppSupportOS2::StartDDE() {
    NS_ENSURE_TRUE( mInstance == 0, NS_ERROR_NOT_INITIALIZED );

    BOOL bDdemlReady = SetupOS2ddeml();
    UINT rc = DDEERR_NOT_INITIALIZED;
    if( bDdemlReady == TRUE )
    {
       rc = (UINT) WinDdeInitialize( 
                                  &mInstance,
                                  nsNativeAppSupportOS2::HandleDDENotification,
                                  APPCLASS_STANDARD, 
                                  0 );
    }

    // Initialize DDE.
    NS_ENSURE_TRUE( DDEERR_NO_ERROR == rc, NS_ERROR_FAILURE );
                         
    
    // Allocate DDE strings.
    NS_ENSURE_TRUE( ( mApplication = WinDdeCreateStringHandle( mAppName, CP_WINANSI ) ) && InitTopicStrings(),
                    NS_ERROR_FAILURE );

    // Next step is to register a DDE service.
    NS_ENSURE_TRUE( WinDdeNameService( mInstance, mApplication, 0, DNS_REGISTER ), NS_ERROR_FAILURE );

#if MOZ_DEBUG_DDE
    printf( "DDE server started\n" );
#endif

    return NS_OK;
}

// If no DDE conversations are pending, terminate DDE.
NS_IMETHODIMP
nsNativeAppSupportOS2::Stop( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_OK;
    *aResult = PR_TRUE;

    Mutex ddeLock( MOZ_STARTUP_MUTEX_NAME );

    if ( ddeLock.Lock( MOZ_DDE_STOP_TIMEOUT ) ) {
        if ( mConversations == 0 ) {
            this->Quit();
        } else {
            *aResult = PR_FALSE;
        }

        ddeLock.Unlock();
    }
    else {
        // No DDE application name specified, but that's OK.  Just
        // forge ahead.
        *aResult = PR_TRUE;
    }


    return rv;
}

// Terminate DDE regardless.
NS_IMETHODIMP
nsNativeAppSupportOS2::Quit() {
    if ( mInstance ) {
        // Unregister application name.
        WinDdeNameService( mInstance, mApplication, 0, DNS_UNREGISTER );
        // Clean up strings.
        if ( mApplication ) {
            WinDdeFreeStringHandle( mApplication );
            mApplication = 0;
        }
        for ( int i = 0; i < topicCount; i++ ) {
            if ( mTopics[i] ) {
                WinDdeFreeStringHandle( mTopics[i] );
                mTopics[i] = 0;
            }
        }
        WinDdeUninitialize( mInstance );
        mInstance = 0;
    }

    return NS_OK;
}

PRBool NS_CanRun()
{
      return PR_TRUE;
}

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
    DWORD len = WinDdeQueryString( hsz, NULL, NULL, CP_WINANSI );
    if ( len ) {
        char buffer[ 256 ];
        WinDdeQueryString( hsz, buffer, sizeof buffer, CP_WINANSI );
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


HDDEDATA APIENTRY
nsNativeAppSupportOS2::HandleDDENotification( ULONG idInst,     // DDEML instance
                                              USHORT uType,     // transaction type
                                              USHORT uFmt,      // clipboard data format
                                              HCONV hconv,      // handle to the conversation
                                              HSZ hsz1,         // handle to a string
                                              HSZ hsz2,         // handle to a string
                                              HDDEDATA hdata,   // handle to a global memory object
                                              ULONG dwData1,    // transaction-specific data
                                              ULONG dwData2 ) { // transaction-specific data

#if MOZ_DEBUG_DDE
    printf( "DDE: uType  =%s\n",      uTypeDesc( uType ).get() );
    printf( "     uFmt   =%u\n",      (unsigned)uFmt );
    printf( "     hconv  =%08x\n",    (int)hconv );
    printf( "     hsz1   =%08x:%s\n", (int)hsz1, hszValue( mInstance, hsz1 ).get() );
    printf( "     hsz2   =%08x:%s\n", (int)hsz2, hszValue( mInstance, hsz2 ).get() );
    printf( "     hdata  =%08x\n",    (int)hdata );
    printf( "     dwData1=%08x\n",    (int)dwData1 );
    printf( "     dwData2=%08x\n",    (int)dwData2 );
#endif

    HDDEDATA result = 0;
    if ( uType & XCLASS_BOOL ) {
        switch ( uType ) {
            case XTYP_CONNECT:
                // Make sure its for our service/topic.
                if ( FindTopic( hsz1 ) != -1 ) {
                    // We support this connection.
                    result = (HDDEDATA)1;
                }
                break;
            case XTYP_CONNECT_CONFIRM:
                // We don't care about the conversation handle, at this point.
                result = (HDDEDATA)1;
                break;
        }
    } else if ( uType & XCLASS_DATA ) {
        if ( uType == XTYP_REQUEST ) {
            switch ( FindTopic( hsz1 ) ) {
                case topicOpenURL: {
                    // Open a given URL...
                    nsCString url = ParseDDEArg( hsz2, 0 );
                    // Make it look like command line args.
                    url.Insert( "mozilla -url ", 0 );
#if MOZ_DEBUG_DDE
                    printf( "Handling dde XTYP_REQUEST request: [%s]...\n", url.get() );
#endif
                    // Now handle it.
                    HandleRequest( LPBYTE( url.get() ), PR_FALSE );
                    // Return pseudo window ID.
                    result = CreateDDEData( 1 );
                    break;
                }
                case topicActivate: {
                    // Activate a Nav window...
                    nsCString windowID = ParseDDEArg( hsz2, 0 );
                    // 4294967295 is decimal for 0xFFFFFFFF which is also a
                    //   correct value to do that Activate last window stuff
                    if ( windowID.Equals( "-1" ) ||
                         windowID.Equals( "4294967295" ) ) {
                        // We only support activating the most recent window (or a new one).
                        ActivateLastWindow();
                        // Return pseudo window ID.
                        result = CreateDDEData( 1 );
                    }
                    break;
                }
                case topicVersion: {
                    // Return version.  We're restarting at 1.0!
                    DWORD version = 1 << 16; // "1.0"
                    result = CreateDDEData( version );
                    break;
                }
                case topicRegisterViewer: {
                    // Register new viewer (not implemented).
                    result = CreateDDEData( PR_FALSE );
                    break;
                }
                case topicUnRegisterViewer: {
                    // Unregister new viewer (not implemented).
                    result = CreateDDEData( PR_FALSE );
                    break;
                }
                default:
                    break;
            }
        } else if ( uType & XTYP_POKE ) {
            switch ( FindTopic( hsz1 ) ) {
                case topicCancelProgress: {
                    // "Handle" progress cancel (actually, pretty much ignored).
                    result = (HDDEDATA)DDE_FACK;
                    break;
                }
                default:
                    break;
            }
        }
    } else if ( uType & XCLASS_FLAGS ) {
        if ( uType == XTYP_EXECUTE ) {
            // Prove that we received the request.
            DWORD bytes;
            LPBYTE request = (LPBYTE)WinDdeAccessData( hdata, &bytes );
#if MOZ_DEBUG_DDE
            printf( "Handling dde request: [%s]...\n", (char*)request );
#endif
            HandleRequest( request );
            result = (HDDEDATA)DDE_FACK;
        } else {
            result = (HDDEDATA)DDE_NOTPROCESSED;
        }
    } else if ( uType & XCLASS_NOTIFICATION ) {
    }
#if MOZ_DEBUG_DDE
    printf( "DDE result=%d (0x%08X)\n", (int)result, (int)result );
#endif
    return result;
}

// Utility to parse out argument from a DDE item string.
nsCString nsNativeAppSupportOS2::ParseDDEArg( HSZ args, int index ) {
    nsCString result;
    DWORD argLen = WinDdeQueryString( args, NULL, NULL, CP_WINANSI );
    if ( argLen ) {
        nsCString temp;
        // Ensure result's buffer is sufficiently big.
        temp.SetLength( argLen + 1 );
        // Now get the string contents.
        WinDdeQueryString( args, (char*)temp.get(), temp.Length(), CP_WINANSI );
        // Parse out the given arg.  We assume there's no commas within quoted strings
        // (which may not be accurate, but makes life so much easier).
        const char *p = temp.get();
        // Skip index commas.
        PRInt32 offset = index ? temp.FindChar( ',', PR_FALSE, 0, index ) : 0;
        if ( offset != -1 ) {
            // Desired argument starts there and ends at next comma.
            PRInt32 end = temp.FindChar( ',', PR_FALSE, offset+1 );
            if ( end == -1 ) {
                // Rest of string.
                end = temp.Length();
            }
            // Extract result.
            result.Assign( temp.get() + offset, end - offset ); 
        }

    }
    return result;
}

void nsNativeAppSupportOS2::ActivateLastWindow() {
    nsCOMPtr<nsIDOMWindowInternal> navWin;
    GetMostRecentWindow( NS_LITERAL_STRING("navigator:browser").get(), getter_AddRefs( navWin ) );
    if ( navWin ) {
        // Activate that window.
        activateWindow( navWin );
    } else {
        // Need to create a Navigator window, then.
        OpenBrowserWindow( "about:blank" );
    }
}

HDDEDATA nsNativeAppSupportOS2::CreateDDEData( DWORD value ) {
    HDDEDATA result = WinDdeCreateDataHandle( (LPBYTE)&value,
                                           sizeof value,
                                           0,
                                           mApplication,
                                           CF_TEXT,
                                           0 );
    return result;
}

// Handle DDE request.  The argument is the command line received by the
// DDE client process.  We convert that string to an nsICmdLineService
// object via GetCmdLineArgs.  Then, we look for certain well-known cmd
// arguments.  This replicates code elsewhere, to some extent,
// unfortunately (if you can fix that, please do).
void
nsNativeAppSupportOS2::HandleRequest( LPBYTE request, PRBool newWindow ) {
    // Parse command line.

    nsCOMPtr<nsICmdLineService> args;
    nsresult rv;

    rv = GetCmdLineArgs( request, getter_AddRefs( args ) );
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsIAppShellService> appShell(do_GetService("@mozilla.org/appshell/appShellService;1", &rv));
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsINativeAppSupport> nativeApp;
    rv = appShell->GetNativeAppSupport(getter_AddRefs( nativeApp ));
    if (NS_FAILED(rv)) return;

    // first see if there is a url
    nsXPIDLCString arg;
    rv = args->GetURLToLoad(getter_Copies(arg));
    if (NS_SUCCEEDED(rv) && (const char*)arg ) {
      // Launch browser.
#if MOZ_DEBUG_DDE
      printf( "Launching browser on url [%s]...\n", (const char*)arg );
#endif
      if (NS_SUCCEEDED(nativeApp->EnsureProfile(args)))
        (void)OpenBrowserWindow( arg );
      return;
    }


    // ok, let's try the -chrome argument
    rv = args->GetCmdLineValue("-chrome", getter_Copies(arg));
    if (NS_SUCCEEDED(rv) && (const char*)arg ) {
      // Launch chrome.
#if MOZ_DEBUG_DDE
      printf( "Launching chrome url [%s]...\n", (const char*)arg );
#endif
      if (NS_SUCCEEDED(nativeApp->EnsureProfile(args)))
        (void)OpenWindow( arg, "" );
      return;
    }

    // try using the command line service to get the url
    nsCString taskURL;
    rv = GetStartupURL(args, taskURL);
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(nativeApp->EnsureProfile(args))) {
      (void)OpenWindow(taskURL, "");
      return;
    }

    // try for the "-kill" argument, to shut down the server
    rv = args->GetCmdLineValue( "-kill", getter_Copies(arg));
    if ( NS_SUCCEEDED(rv) && (const char*)arg ) {
      // Turn off server mode.
      nsCOMPtr<nsIAppShellService> appShell =
        do_GetService( "@mozilla.org/appshell/appShellService;1", &rv);
      if (NS_FAILED(rv)) return;
      
      nsCOMPtr<nsINativeAppSupport> native;
      rv = appShell->GetNativeAppSupport( getter_AddRefs( native ));
      if (NS_SUCCEEDED(rv)) {
        native->SetIsServerMode( PR_FALSE );

        // close app if there are no more top-level windows.
        PRBool quitIfNoWindows = PR_FALSE;
        appShell->GetQuitOnLastWindowClosing( &quitIfNoWindows );
        if (quitIfNoWindows) {
          nsCOMPtr<nsIDOMWindowInternal> win;
          GetMostRecentWindow( 0, getter_AddRefs( win ) );
          if (!win)
            appShell->Quit();
        }
      }

      return;
    }

    // ok, no idea what the param is. 
#if MOZ_DEBUG_DDE
    printf( "Unknown request [%s]\n", (char*) request );
#endif
    // if all else fails, open a browser window
    const char * const contractID =
      "@mozilla.org/commandlinehandler/general-startup;1?type=browser";
    nsCOMPtr<nsICmdLineHandler> handler = do_GetService(contractID, &rv);
    if (NS_FAILED(rv)) return;
    
    rv = nativeApp->EnsureProfile(args);
    if (NS_FAILED(rv)) return;
      
    nsXPIDLString defaultArgs;
    rv = handler->GetDefaultArgs(getter_Copies(defaultArgs));
    if (NS_FAILED(rv) || !defaultArgs) return;
      
    if (defaultArgs) {
      nsCAutoString url;
      url.AssignWithConversion( defaultArgs );
      OpenBrowserWindow((const char*)url);
    } else {
      OpenBrowserWindow("about:blank");
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
    nsCAutoString arg;
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
                    strcpy( argv[argc], arg.get() ); 
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
        printf( "Error creating command line service = 0x%08X (argc=%d, argv=0x%08X)\n", (int)rv, (int)argc, (void*)argv );
#endif
    }

    // Cleanup.
    while ( argc ) {
        delete [] argv[ --argc ];
    }
    delete [] argv;

    return rv;
}

// Check to see if we have a profile. We will not have a profile
// at this point if we were launched invisibly in -turbo mode, and
// the profile mgr needed to show UI (to pick from multiple profiles).
// At this point, we can show UI, so call DoProfileStartUp().

nsresult
nsNativeAppSupportOS2::EnsureProfile(nsICmdLineService* args)
{
  nsresult rv;  

  nsCOMPtr<nsIProfileInternal> profileMgr(do_GetService(NS_PROFILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIAppShellService> appShell(do_GetService("@mozilla.org/appshell/appShellService;1", &rv));
  if (NS_FAILED(rv)) return rv;

  // If we have a profile, everything is fine.
  PRBool haveProfile;
  rv = profileMgr->IsCurrentProfileAvailable(&haveProfile);
  if (NS_SUCCEEDED(rv) && haveProfile)
      return NS_OK;
 
  // If the profile selection is happening, fail.
  PRBool doingProfileStartup;
  rv = profileMgr->GetIsStartingUp(&doingProfileStartup);
  if (NS_FAILED(rv) || doingProfileStartup) return NS_ERROR_FAILURE;
  rv = appShell->DoProfileStartup(args, PR_TRUE);
  return rv;
}

nsresult
nsNativeAppSupportOS2::OpenWindow( const char*urlstr, const char *args ) {

  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  nsCOMPtr<nsISupportsString> sarg(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (sarg)
    sarg->SetData(args);

  if (wwatch && sarg) {
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = wwatch->OpenWindow(0, urlstr, "_blank", "chrome,dialog=no,all",
                   sarg, getter_AddRefs(newWindow));
#if MOZ_DEBUG_DDE
  } else {
      printf("Get WindowWatcher (or create string) failed\n");
#endif
  }
  return rv;
}

static char procPropertyName[] = "MozillaProcProperty";

#ifdef DOWENEED
// Subclass procedure used to filter out WM_SETFOCUS messages while reparenting.
static LRESULT CALLBACK focusFilterProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
    if ( uMsg == WM_SETFOCUS ) {
        // Don't let Mozilla's window procedure see this.
        return 0;
    } else {
        // Pass on all other messages to Mozilla's window proc.
        HANDLE oldProc = ::GetProp( hwnd, procPropertyName );
        if ( oldProc ) {
            return ::CallWindowProc( (WNDPROC)oldProc, hwnd, uMsg, wParam, lParam );
        } else {
            // Last resort is the default window proc.
            return ::DefWindowProc( hwnd, uMsg, wParam, lParam );
        }
    }
}
#endif /* DOWENEED */

HWND hwndForDOMWindow( nsISupports *window ) {
    nsCOMPtr<nsIScriptGlobalObject> ppScriptGlobalObj( do_QueryInterface(window) );
    if ( !ppScriptGlobalObj ) {
        return 0;
    }
    nsCOMPtr<nsIDocShell> ppDocShell;
    ppScriptGlobalObj->GetDocShell( getter_AddRefs( ppDocShell ) );
    if ( !ppDocShell ) {
        return 0;
    }
    nsCOMPtr<nsIBaseWindow> ppBaseWindow( do_QueryInterface( ppDocShell ) );
    if ( !ppBaseWindow ) {
        return 0;
    }

    nsCOMPtr<nsIWidget> ppWidget;
    ppBaseWindow->GetMainWidget( getter_AddRefs( ppWidget ) );

    return (HWND)( ppWidget->GetNativeData( NS_NATIVE_WIDGET ) );
}

nsresult
nsNativeAppSupportOS2::ReParent( nsISupports *window, HWND newParent ) {
    HWND hMainFrame = hwndForDOMWindow( window );
    if ( !hMainFrame ) {
        return NS_ERROR_FAILURE;
    }

#ifdef DOWENEED
    // Filter out WM_SETFOCUS messages while reparenting to 
    // other than the desktop.
    //
    // For some reason, Windows generates one and it causes
    // our focus/activation code to assert.
    LONG oldProc = 0;
    if ( newParent ) {
        // Subclass the window.
        oldProc = ::SetWindowLong( hMainFrame,
                                   GWL_WNDPROC,
                                   (LONG)(WNDPROC)focusFilterProc );

        // Store old procedure in window so it is available within
        // focusFilterProc.
        ::SetProp( hMainFrame, procPropertyName, (HANDLE)oldProc );
    }
#endif /* DOWENEED */

    // Reset the parent.
    WinSetParent( hMainFrame, newParent, FALSE );

#ifdef DOWENEED
    // Restore old procedure.
    if ( newParent ) {
        ::SetWindowLong( hMainFrame, GWL_WNDPROC, oldProc );
        ::RemoveProp( hMainFrame, procPropertyName );
    }
#endif /* DOWENEED */

    return NS_OK;
}

static const char sJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";

class SafeJSContext {
public:
  SafeJSContext();
  ~SafeJSContext();

  nsresult   Push();
  JSContext *get() { return mContext; }

protected:
  nsCOMPtr<nsIThreadJSContextStack>  mService;
  JSContext                         *mContext;
};

SafeJSContext::SafeJSContext() : mContext(nsnull) {
}

SafeJSContext::~SafeJSContext() {
  JSContext *cx;
  nsresult   rv;

  if(mContext) {
    rv = mService->Pop(&cx);
    NS_ASSERTION(NS_SUCCEEDED(rv) && cx == mContext, "JSContext push/pop mismatch");
  }
}

nsresult SafeJSContext::Push() {
  nsresult rv;

  if (mContext) // only once
    return NS_ERROR_FAILURE;

  mService = do_GetService(sJSStackContractID);
  if(mService) {
    rv = mService->GetSafeJSContext(&mContext);
    if (NS_SUCCEEDED(rv) && mContext) {
      rv = mService->Push(mContext);
      if (NS_FAILED(rv))
        mContext = 0;
    }
  }
  return mContext ? NS_OK : NS_ERROR_FAILURE; 
}


nsresult
nsNativeAppSupportOS2::OpenBrowserWindow( const char *args, PRBool newWindow ) {
    nsresult rv = NS_OK;
    // Open the argument URL in the most recently used Navigator window.
    // If there is no Nav window, open a new one.
    
    // Get most recently used Nav window.
    nsCOMPtr<nsIDOMWindowInternal> navWin;
    GetMostRecentWindow( NS_LITERAL_STRING( "navigator:browser" ).get(), getter_AddRefs( navWin ) );

    // This isn't really a loop.  We just use "break" statements to fall
    // out to the OpenWindow call when things go awry.
    do {
        // If caller requires a new window, then don't use an existing one.
        if ( newWindow ) {
            break;
        }
        if ( !navWin ) {
            // Have to open a new one.
            break;
        }
        // Get content window.
        nsCOMPtr<nsIDOMWindow> content;
        navWin->GetContent( getter_AddRefs( content ) );
        if ( !content ) {
            break;
        }
        // Convert that to internal interface.
        nsCOMPtr<nsIDOMWindowInternal> internalContent( do_QueryInterface( content ) );
        if ( !internalContent ) {
            break;
        }
        // Get location.
        nsCOMPtr<nsIDOMLocation> location;
        internalContent->GetLocation( getter_AddRefs( location ) );
        if ( !location ) {
            break;
        }
        // Set up environment.
        SafeJSContext context;
        if ( NS_FAILED( context.Push() ) ) {
            break;
        }
        // Set href.
        nsAutoString url; url.AssignWithConversion( args );
        if ( NS_FAILED( location->SetHref( url ) ) ) {
            break;
        }
        // Finally, if we get here, we're done.
        return NS_OK;
    } while ( PR_FALSE );

    // Last resort is to open a brand new window.
    return OpenWindow( "chrome://navigator/content", args );
}

#ifdef DOWENEED
// Utility function that sets up system tray icon.
void
nsNativeAppSupportWin::SetupSysTrayIcon() {
    // Messages go to the hidden window.
    mIconData.hWnd  = (HWND)MessageWindow();

    // Icon is our default application icon.
    mIconData.hIcon =  ::LoadIcon( ::GetModuleHandle( NULL ), IDI_APPLICATION ),

    // Tooltip is the brand short name.
    mIconData.szTip[0] = 0;
    nsCOMPtr<nsIStringBundleService> svc( do_GetService( NS_STRINGBUNDLE_CONTRACTID ) );
    if ( svc ) {
        nsCOMPtr<nsIStringBundle> bundle1;
        svc->CreateBundle( "chrome://global/locale/brand.properties", getter_AddRefs( bundle1 ) );
        if ( bundle1 ) {
            nsXPIDLString tooltip;
            bundle1->GetStringFromName( NS_LITERAL_STRING( "brandShortName" ).get(),
                                        getter_Copies( tooltip ) );
            // (damned strings...)
            nsAutoString autoTip( tooltip );
            nsAutoCString tip( autoTip );    
            ::strncpy( mIconData.szTip, (const char*)tip, sizeof mIconData.szTip - 1 );
        }
        // Build menu.
        nsCOMPtr<nsIStringBundle> bundle2;
        svc->CreateBundle( "chrome://communicator/locale/profile/profileManager.properties",
                           getter_AddRefs( bundle2 ) );
        nsAutoString exitText;
        if ( bundle2 ) {
            nsXPIDLString text;
            bundle2->GetStringFromName( NS_LITERAL_STRING( "exitButton" ).get(),
                                        getter_Copies( text ) );
            exitText = (const PRUnichar*)text;
        }
        if ( exitText.IsEmpty() ) {
            // Fall back to this.
            exitText = NS_LITERAL_STRING( "Exit" );
        }

        // Create menu and add item.
        mTrayIconMenu = ::CreatePopupMenu();
        ::AppendMenuW( mTrayIconMenu, MF_STRING, TURBO_QUIT, exitText.get() );

    }
    
    // Add the tray icon.
    ::Shell_NotifyIcon( NIM_ADD, &mIconData );
}

// Utility function to remove the system tray icon.
void
nsNativeAppSupportWin::RemoveSysTrayIcon() {
    // Remove the tray icon.
    ::Shell_NotifyIcon( NIM_DELETE, &mIconData );
    // Delete the menu.
    ::DestroyMenu( mTrayIconMenu );
}
#endif /* DOWENEED */



//   This opens a special browser window for purposes of priming the pump for
//   server mode (getting stuff into the caching, loading .dlls, etc.).  The
//   window will have these attributes:
//     - Load about:blank (no home page)
//     - No toolbar (so there's no sidebar panels loaded, either)
//     - Pass magic arg to cause window to close in onload handler.
NS_IMETHODIMP
nsNativeAppSupportOS2::StartServerMode() {
#ifdef DOWENEED
    // Turn on system tray icon.
    SetupSysTrayIcon();
#endif /* DOWENEED */

    // Create some of the objects we'll need.
    nsCOMPtr<nsIWindowWatcher>   ww(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    nsCOMPtr<nsISupportsWString> arg1(do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID));
    nsCOMPtr<nsISupportsWString> arg2(do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID));
    if ( !ww || !arg1 || !arg2 ) {
        return NS_OK;
    }

    // Create the array for the arguments.
    nsCOMPtr<nsISupportsArray>   argArray;
    NS_NewISupportsArray( getter_AddRefs( argArray ) );
    if ( !argArray ) {
        return NS_OK;
    }

    // arg1 is the url to load.
    // arg2 is the string that tells navigator.js to auto-close.
    arg1->SetData( NS_LITERAL_STRING( "about:blank" ).get() );
    arg2->SetData( NS_LITERAL_STRING( "turbo=yes" ).get() );

    // Put args into array.
    if ( NS_FAILED( argArray->AppendElement( arg1 ) ) ||
         NS_FAILED( argArray->AppendElement( arg2 ) ) ) {
        return NS_OK;
    }

    // Now open the window.
    nsCOMPtr<nsIDOMWindow> newWindow;
    ww->OpenWindow( 0,
                    "chrome://navigator/content",
                    "_blank",
                    "chrome,dialog=no,toolbar=no",
                    argArray,
                    getter_AddRefs( newWindow ) );

    if ( !newWindow ) {
        return NS_OK;
    }

    // Hide this window by re-parenting it (to ensure it doesn't appear).
    ReParent( newWindow, (HWND)MessageWindow() );

    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportOS2::SetIsServerMode( PRBool isServerMode ) {
    // If it is being turned off, remove systray icon.
    if ( mServerMode && !isServerMode ) {
#ifdef DOWENEED
        RemoveSysTrayIcon();
#endif /* DOWENEED */
    }
    return nsNativeAppSupportBase::SetIsServerMode( isServerMode );
}

NS_IMETHODIMP
nsNativeAppSupportOS2::OnLastWindowClosing( nsIXULWindow *aWindow ) {
 
    nsresult rv;

    if (!mServerMode)
        return NS_OK;

    // If the last window closed is our special "turbo" window made
    // in StartServerMode(), don't do anything.
    nsCOMPtr<nsIDocShell> docShell;
    (void)aWindow->GetDocShell(getter_AddRefs(docShell));
    nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docShell));
    if (domWindow == mInitialWindow) {
        mInitialWindow = nsnull;
        return NS_OK;
    }

    nsCOMPtr<nsIProfileInternal> profileMgr(do_GetService(NS_PROFILE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = profileMgr->ShutDownCurrentProfile(nsIProfile::SHUTDOWN_PERSIST);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



// go through the command line arguments, and try to load a handler
// for one, and when we do, get the chrome URL for its task
nsresult
nsNativeAppSupportOS2::GetStartupURL(nsICmdLineService *args, nsCString& taskURL)
{
    nsresult rv;
    
    nsCOMPtr<nsICmdLineHandler> handler;

    // see if there is a handler
    rv = args->GetHandlerForParam(nsnull, getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;
    
    // ok, from here on out, failures really are fatal
    nsXPIDLCString url;
    rv = handler->GetChromeUrlForTask(getter_Copies(url));
    if (NS_FAILED(rv)) return rv;

    taskURL.Assign(url.get());
    return NS_OK;
}

