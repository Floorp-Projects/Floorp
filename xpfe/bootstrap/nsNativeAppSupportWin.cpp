/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law       law@netscape.com
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

// For server mode systray icon.
#include "nsIStringBundle.h"

#include "nsNativeAppSupportBase.h"
#include "nsNativeAppSupportWin.h"
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
#include "nsIWindowsHooks.h"
#include "nsIPromptService.h"
#include "nsNetCID.h"
#include "nsIObserverService.h"

// These are needed to load a URL in a browser window.
#include "nsIDOMLocation.h"
#include "nsIJSContextStack.h"
#include "nsIWindowMediator.h"

#include <windows.h>
#include <shellapi.h>
#include <ddeml.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

#define TURBO_NAVIGATOR 1
#define TURBO_MAIL 2
#define TURBO_EDITOR 3
#define TURBO_ADDRESSBOOK 4
#define TURBO_DISABLE 5
#define TURBO_EXIT 6

/*************************************************************/
/*                      MAPI SUPPORT                         */
/*************************************************************/

#define MAPI_DLL               "msgMapi.dll"
#define MAPI_INIT_METHOD       "Init"
#define MAPI_STARTUP_ARG       "/MAPIStartUp"

typedef void (PASCAL InitMethod)();

HINSTANCE mapiInstance = nsnull;

void CheckMapiSupport()
{
    mapiInstance = LoadLibrary(MAPI_DLL);
}

/*************************************************************/

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

static char* GetACPString(const nsString& aStr)
{
    int acplen = aStr.Length() * 2 + 1;
    char * acp = new char[ acplen ];
    if( acp ) {
        int outlen = ::WideCharToMultiByte( CP_ACP, 0, aStr.get(), aStr.Length(),
                                            acp, acplen, NULL, NULL );
        if ( outlen >= 0)
            acp[ outlen ] = '\0';  // null terminate
    }
    return acp;
}

static
void
activateWindow( nsIDOMWindowInternal *win ) {
    // Try to get native window handle.
    HWND hwnd = hwndForDOMWindow( win );
    if ( hwnd ) {
        // Restore the window in case it is minimized.
        ::ShowWindow( hwnd, SW_RESTORE );
        // Use the OS call, if possible.
        ::SetForegroundWindow( hwnd );
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

class nsSplashScreenWin : public nsISplashScreen {
public:
    nsSplashScreenWin();
    ~nsSplashScreenWin();

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
    static nsSplashScreenWin* GetPointer( HWND dlg );

    static BOOL CALLBACK DialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp );
    static DWORD WINAPI ThreadProc( LPVOID );

    HWND mDlg;
    HBITMAP mBitmap;
    nsrefcnt mRefCnt;
}; // class nsSplashScreenWin

// Simple Win32 mutex wrapper.
struct Mutex {
    Mutex( const char *name )
        : mName( name ),
          mHandle( 0 ),
          mState( -1 ) {
        mHandle = CreateMutex( 0, FALSE, mName.get() );
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
    HANDLE    mHandle;
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

class nsNativeAppSupportWin : public nsNativeAppSupportBase {
public:
    // Overrides of base implementation.
    NS_IMETHOD Start( PRBool *aResult );
    NS_IMETHOD Stop( PRBool *aResult );
    NS_IMETHOD Quit();
    NS_IMETHOD StartServerMode();
    NS_IMETHOD OnLastWindowClosing( nsIXULWindow *aWindow );
    NS_IMETHOD SetIsServerMode( PRBool isServerMode );
    NS_IMETHOD StartAddonFeatures();
    NS_IMETHOD StopAddonFeatures();
    NS_IMETHOD EnsureProfile(nsICmdLineService* args);

    // The "old" Start method (renamed).
    NS_IMETHOD StartDDE();

    // Utility function to handle a Win32-specific command line
    // option: "-console", which dynamically creates a Windows
    // console.
    void CheckConsole();

private:
    static HDDEDATA CALLBACK HandleDDENotification( UINT     uType,
                                                    UINT     uFmt,
                                                    HCONV    hconv,
                                                    HSZ      hsz1,
                                                    HSZ      hsz2,
                                                    HDDEDATA hdata,
                                                    ULONG    dwData1,
                                                    ULONG    dwData2 );
    static void HandleRequest( LPBYTE request, PRBool newWindow = PR_TRUE );
    static nsCString ParseDDEArg( HSZ args, int index );
    static void ActivateLastWindow();
    static HDDEDATA CreateDDEData( DWORD value );
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
    static NOTIFYICONDATA mIconData;
    static HMENU          mTrayIconMenu;

    static HSZ   mApplication, mTopics[ topicCount ];
    static DWORD mInstance;
    static char *mAppName;
    static nsIDOMWindow *mInitialWindow;
    static PRBool mDidProfileStartup;
    friend struct MessageWindow;
}; // nsNativeAppSupportWin

nsSplashScreenWin::nsSplashScreenWin()
    : mDlg( 0 ), mBitmap( 0 ), mRefCnt( 0 ) {
}

nsSplashScreenWin::~nsSplashScreenWin() {
#if MOZ_DEBUG_DDE
    printf( "splash screen dtor called\n" );
#endif
    // Make sure dialog is gone.
    Hide();
}

NS_IMETHODIMP
nsSplashScreenWin::Show() {
    // Spawn new thread to display real splash screen.
    DWORD threadID = 0;
    HANDLE handle = CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)ThreadProc, this, 0, &threadID );
    CloseHandle(handle);

    return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenWin::Hide() {
    if ( mDlg ) {
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
        // Dismiss the dialog.
        ::PostMessage(mDlg, WM_CLOSE, (WPARAM)mBitmap, (LPARAM)hWndTopLevel);
        mBitmap = 0;
        mDlg = 0;
    }
    return NS_OK;
}

void
nsSplashScreenWin::LoadBitmap() {
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
}

BOOL CALLBACK
nsSplashScreenWin::DialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp ) {
    if ( msg == WM_INITDIALOG ) {
        // Store dialog handle.
        nsSplashScreenWin *splashScreen = (nsSplashScreenWin*)lp;
        if ( lp ) {
            splashScreen->SetDialog( dlg );

            // Try to load customized bitmap.
            splashScreen->LoadBitmap();
        }

        /* Size and center the splash screen correctly. The flags in the
         * dialog template do not do the right thing if the user's
         * machine is using large fonts.
         */
        HWND bitmapControl = GetDlgItem( dlg, IDB_SPLASH );
        if ( bitmapControl ) {
            HBITMAP hbitmap = (HBITMAP)SendMessage( bitmapControl,
                                                    STM_GETIMAGE,
                                                    IMAGE_BITMAP,
                                                    0 );
            if ( hbitmap ) {
                BITMAP bitmap;
                if ( GetObject( hbitmap, sizeof bitmap, &bitmap ) ) {
                    SetWindowPos( dlg,
                                  NULL,
                                  GetSystemMetrics(SM_CXSCREEN)/2 - bitmap.bmWidth/2,
                                  GetSystemMetrics(SM_CYSCREEN)/2 - bitmap.bmHeight/2,
                                  bitmap.bmWidth,
                                  bitmap.bmHeight,
                                  SWP_NOZORDER );
                    ShowWindow( dlg, SW_SHOW );
                }
            }
        }
        return 1;
    } else if (msg == WM_CLOSE) {
        // Before killing ourself, set the top-level current.
        // See comments in nsSplashScreenWin::Hide() above.
        HWND topLevel = (HWND)lp;
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
    return 0;
}

void nsSplashScreenWin::SetDialog( HWND dlg ) {
    // Save dialog handle.
    mDlg = dlg;
    // Store this pointer in the dialog.
    SetWindowLong( mDlg, DWL_USER, (LONG)(void*)this );
}

nsSplashScreenWin *nsSplashScreenWin::GetPointer( HWND dlg ) {
    // Get result from dialog user data.
    LONG data = GetWindowLong( dlg, DWL_USER );
    return (nsSplashScreenWin*)(void*)data;
}

DWORD WINAPI nsSplashScreenWin::ThreadProc( LPVOID splashScreen ) {
    DialogBoxParam( GetModuleHandle( 0 ),
                    MAKEINTRESOURCE( IDD_SPLASH ),
                    HWND_DESKTOP,
                    (DLGPROC)DialogProc,
                    (LPARAM)splashScreen );
    return 0;
}

void
nsNativeAppSupportWin::CheckConsole() {
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
        } else if ( strcmp( "-turbo", __argv[i] ) == 0
                    ||
                    strcmp( "/turbo", __argv[i] ) == 0
                    ||
                    strcmp( "-server", __argv[i] ) == 0
                    ||
                    strcmp( "/server", __argv[i] ) == 0 ) {
            // Start in server mode (and suppress splash screen).
            mServerMode = PR_TRUE;
            mShouldShowUI = PR_FALSE;
            __argv[i] = "-nosplash"; // Bit of a hack, but it works!
            // Ignore other args.
            break;
        }
    }
    // check if this is a restart of the browser after quiting from
    // the servermoded browser instance.
    if (!mServerMode ) {
        HKEY key;
        LONG result = ::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key );
        if ( result == ERROR_SUCCESS ) {
          BYTE regvalue[_MAX_PATH];
          DWORD type, len = sizeof(regvalue);
          result = ::RegQueryValueEx( key, NS_QUICKLAUNCH_RUN_KEY, NULL, &type, regvalue, &len);
          ::RegCloseKey( key );
          if ( result == ERROR_SUCCESS && len > 0 ) {
              // Make sure the filename in the quicklaunch command matches us
              char fileName[_MAX_PATH];
              int rv = ::GetModuleFileName( NULL, fileName, sizeof fileName );
              if (rv && !PL_strncasecmp(fileName, (const char *)regvalue, PR_MIN(len, strlen(fileName)))) {
                  mServerMode = PR_TRUE;
                  mShouldShowUI = PR_TRUE;
              }
          }
        }
    }

    return;
}


// Create and return an instance of class nsNativeAppSupportWin.
nsresult
NS_CreateNativeAppSupport( nsINativeAppSupport **aResult ) {
    if ( aResult ) {
        nsNativeAppSupportWin *pNative = new nsNativeAppSupportWin;
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

// Create instance of Windows splash screen object.
nsresult
NS_CreateSplashScreen( nsISplashScreen **aResult ) {
    if ( aResult ) {
        *aResult = 0;
        for ( int i = 1; i < __argc; i++ ) {
            if ( strcmp( "-quiet", __argv[i] ) == 0
                 ||
                 strcmp( "/quiet", __argv[i] ) == 0 ) {
                // No splash screen, please.
                return NS_OK;
            }
        }
        *aResult = new nsSplashScreenWin;
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
int   nsNativeAppSupportWin::mConversations = 0;
HSZ   nsNativeAppSupportWin::mApplication   = 0;
HSZ   nsNativeAppSupportWin::mTopics[nsNativeAppSupportWin::topicCount] = { 0 };
DWORD nsNativeAppSupportWin::mInstance      = 0;
nsIDOMWindow* nsNativeAppSupportWin::mInitialWindow = nsnull;
PRBool nsNativeAppSupportWin::mDidProfileStartup = PR_FALSE;

NOTIFYICONDATA nsNativeAppSupportWin::mIconData = { sizeof(NOTIFYICONDATA),
                                                    0,
                                                    1,
                                                    NIF_ICON | NIF_MESSAGE | NIF_TIP,
                                                    WM_USER,
                                                    0,
                                                    0 };
HMENU nsNativeAppSupportWin::mTrayIconMenu = 0;


// Message window encapsulation.
struct MessageWindow {
    // ctor/dtor are simplistic
    MessageWindow() {
        // Try to find window.
        mHandle = ::FindWindow( className(), 0 );
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
            ::_snprintf( classNameBuffer,
                         sizeof classNameBuffer,
                         "%s%s",
                         nsNativeAppSupportWin::mAppName,
                         "MessageWindow" );
            mClassName = classNameBuffer;
        }
        return mClassName;
    }

    // Create: Register class and create window.
    NS_IMETHOD Create() {
        WNDCLASS classStruct = { 0,                          // style
                                 &MessageWindow::WindowProc, // lpfnWndProc
                                 0,                          // cbClsExtra
                                 0,                          // cbWndExtra
                                 0,                          // hInstance
                                 0,                          // hIcon
                                 0,                          // hCursor
                                 0,                          // hbrBackground
                                 0,                          // lpszMenuName
                                 className() };              // lpszClassName

        // Register the window class.
        NS_ENSURE_TRUE( ::RegisterClass( &classStruct ), NS_ERROR_FAILURE );

        // Create the window.
        NS_ENSURE_TRUE( ( mHandle = ::CreateWindow( className(),
                                                    0,          // title
                                                    WS_CAPTION, // style
                                                    0,0,0,0,    // x, y, cx, cy
                                                    0,          // parent
                                                    0,          // menu
                                                    0,          // instance
                                                    0 ) ),      // create struct
                        NS_ERROR_FAILURE );

#if MOZ_DEBUG_DDE
        printf( "Message window = 0x%08X\n", (int)mHandle );
#endif

        return NS_OK;
    }

    // SendRequest: Pass string via WM_COPYDATA to message window.
    NS_IMETHOD SendRequest( const char *cmd ) {
        COPYDATASTRUCT cds = { 0, ::strlen( cmd ) + 1, (void*)cmd };
        ::SendMessage( mHandle, WM_COPYDATA, 0, (LPARAM)&cds );
        return NS_OK;
    }

    // Window proc.
    static long CALLBACK WindowProc( HWND msgWindow, UINT msg, WPARAM wp, LPARAM lp ) {
        if ( msg == WM_COPYDATA ) {
            // This is an incoming request.
            COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lp;
#if MOZ_DEBUG_DDE
            printf( "Incoming request: %s\n", (const char*)cds->lpData );
#endif
            (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)cds->lpData );
 } else if ( msg == WM_USER ) {
     if ( lp == WM_RBUTTONUP ) {
         // Show menu with Exit disabled/enabled appropriately.
         nsCOMPtr<nsIDOMWindowInternal> win;
         GetMostRecentWindow( 0, getter_AddRefs( win ) );
         ::EnableMenuItem( nsNativeAppSupportWin::mTrayIconMenu, TURBO_EXIT, win ? MF_GRAYED : MF_ENABLED );
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
         case TURBO_NAVIGATOR:
             (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)"mozilla" );
             break;
         case TURBO_MAIL:
             (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)"mozilla -mail" );
              break;
         case TURBO_EDITOR:
             (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)"mozilla -editor" );
             break;
         case TURBO_ADDRESSBOOK:
             (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)"mozilla -addressbook" );
             break;
         case TURBO_EXIT:
             (void)nsNativeAppSupportWin::HandleRequest( (LPBYTE)"mozilla -kill" );
             break;
         case TURBO_DISABLE:
             nsresult rv;
             nsCOMPtr<nsIWindowsHooks> winHooksService ( do_GetService( NS_IWINDOWSHOOKS_CONTRACTID, &rv ) );
             if ( NS_SUCCEEDED( rv ) )
                 winHooksService->StartupTurboDisable();
             nsCOMPtr<nsIStringBundleService> stringBundleService( do_GetService( NS_STRINGBUNDLE_CONTRACTID ) );
             nsCOMPtr<nsIStringBundle> turboMenuBundle;
             nsCOMPtr<nsIStringBundle> brandBundle;
             if ( stringBundleService ) {
                 stringBundleService->CreateBundle( "chrome://global/locale/brand.properties", getter_AddRefs( brandBundle ) );
                 stringBundleService->CreateBundle( "chrome://navigator/locale/turboMenu.properties",
                                                    getter_AddRefs( turboMenuBundle ) );
             }
             nsXPIDLString dialogMsg;
             nsXPIDLString dialogTitle;
             nsXPIDLString brandName;
             if ( brandBundle && turboMenuBundle ) {
                 brandBundle->GetStringFromName( NS_LITERAL_STRING( "brandShortName" ).get(),
                                                 getter_Copies( brandName ) );
                 const PRUnichar *formatStrings[] = { brandName.get() };
                 turboMenuBundle->GetStringFromName( NS_LITERAL_STRING( "DisableDlgMsg" ).get(),
                                                     getter_Copies( dialogMsg ) );
                 turboMenuBundle->FormatStringFromName( NS_LITERAL_STRING( "DisableDlgTitle" ).get(), formatStrings,
                                                        1, getter_Copies( dialogTitle ) );
             }
             if ( dialogMsg.get() && dialogTitle.get() && brandName.get() ) {
                 nsCOMPtr<nsIPromptService> dialog( do_GetService( "@mozilla.org/embedcomp/prompt-service;1" ) );
                 if ( dialog )
                     dialog->Alert( nsnull, dialogTitle.get(), dialogMsg.get() );
             }
             nsCOMPtr<nsIAppShellService> appShell = do_GetService( "@mozilla.org/appshell/appShellService;1", &rv );
             if ( NS_SUCCEEDED( rv ) ) {
                 nsCOMPtr<nsINativeAppSupport> native;
                 rv = appShell->GetNativeAppSupport( getter_AddRefs( native ) );
                 if ( NS_SUCCEEDED( rv ) )
                     native->SetIsServerMode( PR_FALSE );
                 if ( !win )
                     appShell->Quit();
             }
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
  return TRUE;
}

private:
    HWND mHandle;
}; // struct MessageWindow

static char nameBuffer[128] = { 0 };
char *nsNativeAppSupportWin::mAppName = nameBuffer;

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
nsNativeAppSupportWin::Start( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance == 0, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_ERROR_FAILURE;
    *aResult = PR_FALSE;

    // Grab mutex first.
	int retval;
	UINT id = ID_DDE_APPLICATION_NAME;
	retval = LoadString( (HINSTANCE) NULL, id, (LPTSTR) nameBuffer, sizeof(nameBuffer) );
    if ( retval == 0 ) {
        // No app name; just keep running.
        *aResult = PR_TRUE;
        return NS_OK;
    }

    // Build mutex name from app name.
    char mutexName[128];
    ::_snprintf( mutexName, sizeof mutexName, "%s%s", nameBuffer, MOZ_STARTUP_MUTEX_NAME );
    Mutex startupLock = Mutex( mutexName );

    NS_ENSURE_TRUE( startupLock.Lock( MOZ_DDE_START_TIMEOUT ), NS_ERROR_FAILURE );

    // Search for existing message window.
    MessageWindow msgWindow;
    if ( (HWND)msgWindow ) {
        // We are a client process.  Pass request to message window.
        LPTSTR cmd = ::GetCommandLine();
        rv = msgWindow.SendRequest( cmd );
    } else {
        // We will be server.
        rv = msgWindow.Create();
        if ( NS_SUCCEEDED( rv ) ) {
            // Start up DDE server.
            this->StartDDE();
            // Tell caller to spin message loop.
            *aResult = PR_TRUE;
            CheckMapiSupport();
        }
    }

    startupLock.Unlock();

    return rv;
}

PRBool
nsNativeAppSupportWin::InitTopicStrings() {
    for ( int i = 0; i < topicCount; i++ ) {
        if ( !( mTopics[ i ] = DdeCreateStringHandle( mInstance, topicNames[ i ], CP_WINANSI ) ) ) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

int
nsNativeAppSupportWin::FindTopic( HSZ topic ) {
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
nsNativeAppSupportWin::StartDDE() {
    NS_ENSURE_TRUE( mInstance == 0, NS_ERROR_NOT_INITIALIZED );

    // Initialize DDE.
    NS_ENSURE_TRUE( DMLERR_NO_ERROR == DdeInitialize( &mInstance,
                                                      nsNativeAppSupportWin::HandleDDENotification,
                                                      APPCLASS_STANDARD,
                                                      0 ),
                    NS_ERROR_FAILURE );

    // Allocate DDE strings.
    NS_ENSURE_TRUE( ( mApplication = DdeCreateStringHandle( mInstance, mAppName, CP_WINANSI ) ) && InitTopicStrings(),
                    NS_ERROR_FAILURE );

    // Next step is to register a DDE service.
    NS_ENSURE_TRUE( DdeNameService( mInstance, mApplication, 0, DNS_REGISTER ), NS_ERROR_FAILURE );

#if MOZ_DEBUG_DDE
    printf( "DDE server started\n" );
#endif

    return NS_OK;
}

// If no DDE conversations are pending, terminate DDE.
NS_IMETHODIMP
nsNativeAppSupportWin::Stop( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_OK;
    *aResult = PR_TRUE;

    StopAddonFeatures();

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
nsNativeAppSupportWin::Quit() {
    if ( mInstance ) {
        // Unregister application name.
        DdeNameService( mInstance, mApplication, 0, DNS_UNREGISTER );
        // Clean up strings.
        if ( mApplication ) {
            DdeFreeStringHandle( mInstance, mApplication );
            mApplication = 0;
        }
        for ( int i = 0; i < topicCount; i++ ) {
            if ( mTopics[i] ) {
                DdeFreeStringHandle( mInstance, mTopics[i] );
                mTopics[i] = 0;
            }
        }
        DdeUninitialize( mInstance );
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
nsNativeAppSupportWin::HandleDDENotification( UINT uType,       // transaction type
                                              UINT uFmt,        // clipboard data format
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
#if MOZ_DEBUG_DDE
    printf( "DDE result=%d (0x%08X)\n", (int)result, (int)result );
#endif
    return result;
}

// Utility to parse out argument from a DDE item string.
nsCString nsNativeAppSupportWin::ParseDDEArg( HSZ args, int index ) {
    nsCString result;
    DWORD argLen = DdeQueryString( mInstance, args, NULL, NULL, CP_WINANSI );
    if ( argLen ) {
        nsCString temp;
        // Ensure result's buffer is sufficiently big.
        temp.SetLength( argLen + 1 );
        // Now get the string contents.
        DdeQueryString( mInstance, args, (char*)temp.get(), temp.Length(), CP_WINANSI );
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

void nsNativeAppSupportWin::ActivateLastWindow() {
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

HDDEDATA nsNativeAppSupportWin::CreateDDEData( DWORD value ) {
    HDDEDATA result = DdeCreateDataHandle( mInstance,
                                           (LPBYTE)&value,
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
nsNativeAppSupportWin::HandleRequest( LPBYTE request, PRBool newWindow ) {
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

    // check wheather it is a MAPI request.  If yes, don't open any new
    // windows and just return.
    rv = args->GetCmdLineValue(MAPI_STARTUP_ARG, getter_Copies(arg));
    if (NS_SUCCEEDED(rv) && (const char*)arg) return;


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
nsNativeAppSupportWin::GetCmdLineArgs( LPBYTE request, nsICmdLineService **aResult ) {
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
nsNativeAppSupportWin::EnsureProfile(nsICmdLineService* args)
{
  nsresult rv;

  nsCOMPtr<nsIProfileInternal> profileMgr(do_GetService(NS_PROFILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIAppShellService> appShell(do_GetService("@mozilla.org/appshell/appShellService;1", &rv));
  if (NS_FAILED(rv)) return rv;

  // If we have a profile, everything is fine.
  PRBool haveProfile;
  rv = profileMgr->IsCurrentProfileAvailable(&haveProfile);
  if (mDidProfileStartup && NS_SUCCEEDED(rv) && haveProfile)
      return NS_OK;

  // If the profile selection is happening, fail.
  PRBool doingProfileStartup;
  rv = profileMgr->GetIsStartingUp(&doingProfileStartup);
  if (NS_FAILED(rv) || doingProfileStartup) return NS_ERROR_FAILURE;

  rv = appShell->DoProfileStartup(args, PR_TRUE);

  mDidProfileStartup = PR_TRUE;

  return rv;
}

nsresult
nsNativeAppSupportWin::OpenWindow( const char*urlstr, const char *args ) {

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
nsNativeAppSupportWin::ReParent( nsISupports *window, HWND newParent ) {
    HWND hMainFrame = hwndForDOMWindow( window );
    if ( !hMainFrame ) {
        return NS_ERROR_FAILURE;
    }

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

    // Reset the parent.
    ::SetParent( hMainFrame, newParent );

    // Restore old procedure.
    if ( newParent ) {
        ::SetWindowLong( hMainFrame, GWL_WNDPROC, oldProc );
        ::RemoveProp( hMainFrame, procPropertyName );
    }

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
nsNativeAppSupportWin::OpenBrowserWindow( const char *args, PRBool newWindow ) {
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

void AppendMenuItem( HMENU& menu, PRInt32 aIdentifier, const nsString& aText ) {
  char* ACPText = GetACPString( aText );
  if ( ACPText ) {
    ::AppendMenu( menu, MF_STRING, aIdentifier, ACPText );
    delete [] ACPText;
  }
}

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
        nsCOMPtr<nsIStringBundle> brandBundle;
        nsXPIDLString tooltip;
        svc->CreateBundle( "chrome://global/locale/brand.properties", getter_AddRefs( brandBundle ) );
        if ( brandBundle ) {
            brandBundle->GetStringFromName( NS_LITERAL_STRING( "brandShortName" ).get(),
                                            getter_Copies( tooltip ) );
            // (damned strings...)
            nsAutoString autoTip( tooltip );
            nsAutoCString tip( autoTip );
            ::strncpy( mIconData.szTip, (const char*)tip, sizeof mIconData.szTip - 1 );
        }
        // Build menu.
        nsCOMPtr<nsIStringBundle> turboBundle;
        nsCOMPtr<nsIStringBundle> mailBundle;
        svc->CreateBundle( "chrome://navigator/locale/turboMenu.properties",
                           getter_AddRefs( turboBundle ) );
        nsresult rv = svc->CreateBundle( "chrome://messenger/locale/mailTurboMenu.properties",
                                         getter_AddRefs( mailBundle ) );
        PRBool isMail = NS_SUCCEEDED(rv) && mailBundle;
        nsAutoString exitText;
        nsAutoString disableText;
        nsAutoString navigatorText;
        nsAutoString editorText;
        nsAutoString mailText;
        nsAutoString addressbookText;
        nsXPIDLString text;
        if ( turboBundle ) {
            if ( brandBundle ) {
                const PRUnichar* formatStrings[] = { tooltip.get() };
                turboBundle->FormatStringFromName( NS_LITERAL_STRING( "Exit" ).get(), formatStrings, 1,
                                                   getter_Copies( text ) );
                exitText = (const PRUnichar*)text;
            }
            turboBundle->GetStringFromName( NS_LITERAL_STRING( "Disable" ).get(),
                                            getter_Copies( text ) );
            disableText = (const PRUnichar*)text;
            turboBundle->GetStringFromName( NS_LITERAL_STRING( "Navigator" ).get(),
                                            getter_Copies( text ) );
            navigatorText = (const PRUnichar*)text;
            turboBundle->GetStringFromName( NS_LITERAL_STRING( "Editor" ).get(),
                                            getter_Copies( text ) );
            editorText = (const PRUnichar*)text;
        }
        if (isMail) {
            mailBundle->GetStringFromName( NS_LITERAL_STRING( "MailNews" ).get(),
			                               getter_Copies( text ) );
            mailText = (const PRUnichar*)text;
            mailBundle->GetStringFromName( NS_LITERAL_STRING( "Addressbook" ).get(),
			                               getter_Copies( text ) );
            addressbookText = (const PRUnichar*)text;
        }

        if ( exitText.IsEmpty() )
            exitText = NS_LITERAL_STRING( "&Exit Mozilla" );

        if ( disableText.IsEmpty() )
            disableText = NS_LITERAL_STRING( "&Disable Quick Launch" );

        if ( navigatorText.IsEmpty() )
            navigatorText = NS_LITERAL_STRING( "&Navigator" );

        if ( editorText.IsEmpty() )
            editorText = NS_LITERAL_STRING( "&Composer" );

        if ( isMail ) {
            if ( mailText.IsEmpty() )
              mailText = NS_LITERAL_STRING( "&Mail && Newsgroups" );
            if ( addressbookText.IsEmpty() )
              addressbookText = NS_LITERAL_STRING( "&Address Book" );
        }
        // Create menu and add item.
        mTrayIconMenu = ::CreatePopupMenu();
        ::AppendMenuW( mTrayIconMenu, MF_STRING, TURBO_NAVIGATOR, navigatorText.get() );
        if ( ::GetLastError() == ERROR_CALL_NOT_IMPLEMENTED ) {
            AppendMenuItem( mTrayIconMenu, TURBO_NAVIGATOR, navigatorText );
            if ( isMail )
                AppendMenuItem( mTrayIconMenu, TURBO_MAIL, mailText );
            AppendMenuItem( mTrayIconMenu, TURBO_EDITOR, editorText );
            if ( isMail )
                AppendMenuItem( mTrayIconMenu, TURBO_ADDRESSBOOK, addressbookText );
            ::AppendMenu( mTrayIconMenu, MF_SEPARATOR, NULL, NULL );
            AppendMenuItem( mTrayIconMenu, TURBO_DISABLE, disableText );
            AppendMenuItem( mTrayIconMenu, TURBO_EXIT, exitText );
        }
        else {
            if (isMail)
                ::AppendMenuW( mTrayIconMenu, MF_STRING, TURBO_MAIL, mailText.get() );
            ::AppendMenuW( mTrayIconMenu, MF_STRING, TURBO_EDITOR, editorText.get() );
            if (isMail)
                ::AppendMenuW( mTrayIconMenu, MF_STRING, TURBO_ADDRESSBOOK, addressbookText.get() );
            ::AppendMenuW( mTrayIconMenu, MF_SEPARATOR, NULL, NULL );
            ::AppendMenuW( mTrayIconMenu, MF_STRING, TURBO_DISABLE, disableText.get() );
            ::AppendMenuW( mTrayIconMenu, MF_STRING, TURBO_EXIT, exitText.get() );
        }
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



//   This opens a special browser window for purposes of priming the pump for
//   server mode (getting stuff into the caching, loading .dlls, etc.).  The
//   window will have these attributes:
//     - Load about:blank (no home page)
//     - No toolbar (so there's no sidebar panels loaded, either)
//     - Pass magic arg to cause window to close in onload handler.
NS_IMETHODIMP
nsNativeAppSupportWin::StartServerMode() {

    // Turn on system tray icon.
    SetupSysTrayIcon();

    if (mShouldShowUI) {
        // We dont have to anything anymore. The native UI
        // will create the window
        return NS_OK;
    }

    // Since native UI wont create any window, we create a hidden window
    // so thing work alright.

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
    mInitialWindow = newWindow;
    
    // Hide this window by re-parenting it (to ensure it doesn't appear).
    ReParent( newWindow, (HWND)MessageWindow() );

    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportWin::SetIsServerMode( PRBool isServerMode ) {
    // If it is being turned off, remove systray icon.
    if ( mServerMode && !isServerMode ) {
        RemoveSysTrayIcon();
    }
    else if ( !mServerMode && isServerMode) {
        SetupSysTrayIcon();
    }
    return nsNativeAppSupportBase::SetIsServerMode( isServerMode );
}

NS_IMETHODIMP
nsNativeAppSupportWin::OnLastWindowClosing( nsIXULWindow *aWindow ) {

    if ( !mServerMode )
        return NS_OK;

    // If the last window closed is our special "turbo" window made
    // in StartServerMode(), don't do anything.
    nsCOMPtr<nsIDocShell> docShell;
    ( void )aWindow->GetDocShell( getter_AddRefs( docShell ) );
    nsCOMPtr<nsIDOMWindow> domWindow( do_GetInterface( docShell ) );
    if ( domWindow == mInitialWindow ) {
        mInitialWindow = nsnull;
        return NS_OK;
    }
    nsresult rv;
    if ( !mShownTurboDialog ) {
        PRBool showDialog = PR_TRUE;
        nsCOMPtr<nsIPref> prefService ( do_GetService( NS_PREF_CONTRACTID, &rv ) );
        if ( NS_SUCCEEDED( rv ) )
            prefService->GetBoolPref( "browser.turbo.showDialog", &showDialog );
        nsCOMPtr<nsIDOMWindowInternal> domWindowInt ( do_GetInterface( docShell ) );
        if ( showDialog && domWindowInt ) {
            nsCOMPtr<nsIDOMWindow> newWindow;
            mShownTurboDialog = PR_TRUE;
            domWindowInt->OpenDialog( NS_LITERAL_STRING( "chrome://navigator/content/turboDialog.xul" ),
                                      NS_LITERAL_STRING( "_blank" ),
                                      NS_LITERAL_STRING( "chrome,modal,titlebar,centerscreen,dialog" ),
                                      nsnull, getter_AddRefs( newWindow ) );
        }
    }

    nsCOMPtr<nsIObserverService> observerService(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    observerService->Notify(nsnull, NS_LITERAL_STRING("session-logout").get(), nsnull);

    mDidProfileStartup = PR_FALSE;

    return NS_OK;
}

// go through the command line arguments, and try to load a handler
// for one, and when we do, get the chrome URL for its task
nsresult
nsNativeAppSupportWin::GetStartupURL(nsICmdLineService *args, nsCString& taskURL)
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

/***************************************************************/
/*                        MAPI SUPPORT                         */
/***************************************************************/

NS_IMETHODIMP
nsNativeAppSupportWin::StartAddonFeatures()
{
    InitMethod *InitFunction = nsnull;
    if (mapiInstance != nsnull)
    {
        InitFunction = (InitMethod *)GetProcAddress(mapiInstance, MAPI_INIT_METHOD);
        if (InitFunction != nsnull)
            InitFunction();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportWin::StopAddonFeatures()
{
    if (mapiInstance != nsnull)
    {
        FreeLibrary(mapiInstance);
        mapiInstance = nsnull;
    }

    return NS_OK;
}

/***************************************************************/
