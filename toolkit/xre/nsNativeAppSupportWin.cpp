/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeAppSupportBase.h"
#include "nsNativeAppSupportWin.h"
#include "nsAppRunner.h"
#include "nsXULAppAPI.h"
#include "nsString.h"
#include "nsIBrowserDOMWindow.h"
#include "nsICommandLineRunner.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIDOMChromeWindow.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsArray.h"
#include "nsIWindowWatcher.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsIXULWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPromptService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIDOMLocation.h"
#include "nsIJSContextStack.h"
#include "nsIWebNavigation.h"
#include "nsIWindowMediator.h"
#include "nsNativeCharsetUtils.h"
#include "nsIAppStartup.h"

#include <windows.h>
#include <shellapi.h>
#include <ddeml.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <fcntl.h>

static HWND hwndForDOMWindow( nsISupports * );

static
nsresult
GetMostRecentWindow(const PRUnichar* aType, nsIDOMWindow** aWindow) {
    nsresult rv;
    nsCOMPtr<nsIWindowMediator> med( do_GetService( NS_WINDOWMEDIATOR_CONTRACTID, &rv ) );
    if ( NS_FAILED( rv ) )
        return rv;

    if ( med )
        return med->GetMostRecentWindow( aType, aWindow );

    return NS_ERROR_FAILURE;
}

static
void
activateWindow( nsIDOMWindow *win ) {
    // Try to get native window handle.
    HWND hwnd = hwndForDOMWindow( win );
    if ( hwnd ) {
        // Restore the window if it is minimized.
        if ( ::IsIconic( hwnd ) ) {
            ::ShowWindow( hwnd, SW_RESTORE );
        }
        // Use the OS call, if possible.
        ::SetForegroundWindow( hwnd );
    } else {
        // Use internal method.
        win->Focus();
    }
}


#ifdef DEBUG_law
#undef MOZ_DEBUG_DDE
#define MOZ_DEBUG_DDE 1
#endif

// Simple Win32 mutex wrapper.
struct Mutex {
    Mutex( const PRUnichar *name )
        : mName( name ),
          mHandle( 0 ),
          mState( -1 ) {
        mHandle = CreateMutexW( 0, FALSE, mName.get() );
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
            printf( "...wait complete, result = 0x%08X, GetLastError=0x%08X\n", (int)mState, (int)::GetLastError() );
#endif
            return mState == WAIT_OBJECT_0 || mState == WAIT_ABANDONED;
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
    nsString  mName;
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

/* Update 2007 January
 *
 * A change in behavior was implemented in July 2004 which made the
 * application on launch to add and on quit to remove the ddexec registry key.
 * See bug 246078.
 * Windows Vista has changed the methods used to set an application as default
 * and the new methods are incompatible with removing the ddeexec registry key.
 * See bug 353089.
 *
 * OS DDE Sequence:
 * 1. OS checks if the dde name is registered.
 * 2. If it is registered the OS sends a DDE request with the WWW_OpenURL topic
 *    and the params as specified in the default value of the ddeexec registry
 *    key for the verb (e.g. open).
 * 3. If it isn't registered the OS launches the executable defined in the
 *    verb's (e.g. open) command registry key.
 * 4. If the ifexec registry key is not present the OS sends a DDE request with
 *    the WWW_OpenURL topic and the params as specified in the default value of
 *    the ddeexec registry key for the verb (e.g. open).
 * 5. If the ifexec registry key is present the OS sends a DDE request with the
 *    WWW_OpenURL topic and the params as specified in the ifexec registry key
 *    for the verb (e.g. open).
 *
 * Application DDE Sequence:
 * 1. If the application is running a DDE request is received with the
 *    WWW_OpenURL topic and the params as specified in the default value of the
 *    ddeexec registry key (e.g. "%1",,0,0,,,, where '%1' is the url to open)
 *    for the verb (e.g. open).
 * 2. If the application is not running it is launched with the -requestPending
 *    and the -url argument.
 * 2.1  If the application does not need to restart and the -requestPending
 *      argument is present the accompanying url will not be used. Instead the
 *      application will wait for the DDE message to open the url.
 * 2.2  If the application needs to restart the -requestPending argument is
 *      removed from the arguments used to restart the application and the url
 *      will be handled normally.
 *
 * Note: Due to a bug in IE the ifexec key should not be used (see bug 355650).
 */

class nsNativeAppSupportWin : public nsNativeAppSupportBase,
                              public nsIObserver
{
public:
    NS_DECL_NSIOBSERVER
    NS_DECL_ISUPPORTS_INHERITED

    // Overrides of base implementation.
    NS_IMETHOD Start( bool *aResult );
    NS_IMETHOD Stop( bool *aResult );
    NS_IMETHOD Quit();
    NS_IMETHOD Enable();
    // The "old" Start method (renamed).
    NS_IMETHOD StartDDE();
    // Utility function to handle a Win32-specific command line
    // option: "-console", which dynamically creates a Windows
    // console.
    void CheckConsole();

private:
    static void HandleCommandLine(const char* aCmdLineString, nsIFile* aWorkingDir, PRUint32 aState);
    static HDDEDATA CALLBACK HandleDDENotification( UINT     uType,
                                                    UINT     uFmt,
                                                    HCONV    hconv,
                                                    HSZ      hsz1,
                                                    HSZ      hsz2,
                                                    HDDEDATA hdata,
                                                    ULONG_PTR dwData1,
                                                    ULONG_PTR dwData2 );
    static void ParseDDEArg( HSZ args, int index, nsString& string);
    static void ParseDDEArg( const WCHAR* args, int index, nsString& aString);
    static HDDEDATA CreateDDEData( DWORD value );
    static HDDEDATA CreateDDEData( LPBYTE value, DWORD len );
    static bool     InitTopicStrings();
    static int      FindTopic( HSZ topic );
    static void ActivateLastWindow();
    static nsresult OpenWindow( const char *urlstr, const char *args );
    static nsresult OpenBrowserWindow();
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
        topicGetWindowInfo,
        // Note: Insert new values above this line!!!!!
        topicCount // Count of the number of real topics
    };
    static HSZ   mApplication, mTopics[ topicCount ];
    static DWORD mInstance;
    static bool mCanHandleRequests;
    static PRUnichar mMutexName[];
    friend struct MessageWindow;
}; // nsNativeAppSupportWin

NS_INTERFACE_MAP_BEGIN(nsNativeAppSupportWin)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsNativeAppSupportBase)

NS_IMPL_ADDREF_INHERITED(nsNativeAppSupportWin, nsNativeAppSupportBase)
NS_IMPL_RELEASE_INHERITED(nsNativeAppSupportWin, nsNativeAppSupportBase)

void
nsNativeAppSupportWin::CheckConsole() {
    // Try to attach console to the parent process.
    // It will succeed when the parent process is a command line,
    // so that stdio will be displayed in it.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // Change std handles to refer to new console handles. Before doing so,
        // ensure that stdout/stderr haven't been redirected to a valid file
        if (_fileno(stdout) == -1 || _get_osfhandle(fileno(stdout)) == -1)
            freopen("CONOUT$", "w", stdout);
        // There isn't any `CONERR$`, so that we merge stderr into CONOUT$
        // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683231%28v=vs.85%29.aspx
        if (_fileno(stderr) == -1 || _get_osfhandle(fileno(stderr)) == -1)
            freopen("CONOUT$", "w", stderr);
        if (_fileno(stdin) == -1 || _get_osfhandle(fileno(stdin)) == -1)
            freopen("CONIN$", "r", stdin);
    }

    for ( int i = 1; i < gArgc; i++ ) {
        if ( strcmp( "-console", gArgv[i] ) == 0
             ||
             strcmp( "/console", gArgv[i] ) == 0 ) {
            // Users wants to make sure we have a console.
            // Try to allocate one.
            BOOL rc = ::AllocConsole();
            if ( rc ) {
                // Console allocated.  Fix it up so that output works in
                // all cases.  See http://support.microsoft.com/support/kb/articles/q105/3/05.asp.

                // stdout
                int hCrt = ::_open_osfhandle( (intptr_t)GetStdHandle( STD_OUTPUT_HANDLE ),
                                            _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = ::_fdopen( hCrt, "w" );
                    if ( hf ) {
                        *stdout = *hf;
#ifdef DEBUG
                        ::fprintf( stdout, "stdout directed to dynamic console\n" );
#endif
                    }
                }

                // stderr
                hCrt = ::_open_osfhandle( (intptr_t)::GetStdHandle( STD_ERROR_HANDLE ),
                                          _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = ::_fdopen( hCrt, "w" );
                    if ( hf ) {
                        *stderr = *hf;
#ifdef DEBUG
                        ::fprintf( stderr, "stderr directed to dynamic console\n" );
#endif
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
            // Remove the console argument from the command line.
            do {
                gArgv[i] = gArgv[i + 1];
                ++i;
            } while (gArgv[i]);

            --gArgc;

            // Don't bother doing this more than once.
            break;
        }
    }

    return;
}


// Create and return an instance of class nsNativeAppSupportWin.
nsresult
NS_CreateNativeAppSupport( nsINativeAppSupport **aResult ) {
    nsNativeAppSupportWin *pNative = new nsNativeAppSupportWin;
    if (!pNative) return NS_ERROR_OUT_OF_MEMORY;

    // Check for dynamic console creation request.
    pNative->CheckConsole();

    *aResult = pNative;
    NS_ADDREF( *aResult );

    return NS_OK;
}

// Constants
#define MOZ_DDE_APPLICATION    "Mozilla"
#define MOZ_MUTEX_NAMESPACE    L"Local\\"
#define MOZ_STARTUP_MUTEX_NAME L"StartupMutex"
#define MOZ_DDE_START_TIMEOUT 30000
#define MOZ_DDE_STOP_TIMEOUT  15000
#define MOZ_DDE_EXEC_TIMEOUT  15000

// The array entries must match the enum ordering!
const char * const topicNames[] = { "WWW_OpenURL",
                                    "WWW_Activate",
                                    "WWW_CancelProgress",
                                    "WWW_Version",
                                    "WWW_RegisterViewer",
                                    "WWW_UnRegisterViewer",
                                    "WWW_GetWindowInfo" };

// Static member definitions.
int   nsNativeAppSupportWin::mConversations = 0;
HSZ   nsNativeAppSupportWin::mApplication   = 0;
HSZ   nsNativeAppSupportWin::mTopics[nsNativeAppSupportWin::topicCount] = { 0 };
DWORD nsNativeAppSupportWin::mInstance      = 0;
bool nsNativeAppSupportWin::mCanHandleRequests   = false;

PRUnichar nsNativeAppSupportWin::mMutexName[ 128 ] = { 0 };


// Message window encapsulation.
struct MessageWindow {
    // ctor/dtor are simplistic
    MessageWindow() {
        // Try to find window.
        mHandle = ::FindWindowW( className(), 0 );
    }

    // Act like an HWND.
    operator HWND() {
        return mHandle;
    }

    // Class name: appName + "MessageWindow"
    static const PRUnichar *className() {
        static PRUnichar classNameBuffer[128];
        static PRUnichar *mClassName = 0;
        if ( !mClassName ) {
            ::_snwprintf(classNameBuffer,
                         128,   // size of classNameBuffer in PRUnichars
                         L"%s%s",
                         NS_ConvertUTF8toUTF16(gAppData->name).get(),
                         L"MessageWindow" );
            mClassName = classNameBuffer;
        }
        return mClassName;
    }

    // Create: Register class and create window.
    NS_IMETHOD Create() {
        WNDCLASSW classStruct = { 0,                          // style
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
        NS_ENSURE_TRUE( ::RegisterClassW( &classStruct ), NS_ERROR_FAILURE );

        // Create the window.
        NS_ENSURE_TRUE( ( mHandle = ::CreateWindowW(className(),
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

    // Destory:  Get rid of window and reset mHandle.
    NS_IMETHOD Destroy() {
        nsresult retval = NS_OK;

        if ( mHandle ) {
            // DestroyWindow can only destroy windows created from
            //  the same thread.
            BOOL desRes = DestroyWindow( mHandle );
            if ( FALSE != desRes ) {
                mHandle = NULL;
            }
            else {
                retval = NS_ERROR_FAILURE;
            }
        }

        return retval;
    }

    // SendRequest: Pass the command line via WM_COPYDATA to message window.
    NS_IMETHOD SendRequest() {
        WCHAR *cmd = ::GetCommandLineW();
        WCHAR cwd[MAX_PATH];
        _wgetcwd(cwd, MAX_PATH);

        // Construct a narrow UTF8 buffer <commandline>\0<workingdir>\0
        NS_ConvertUTF16toUTF8 utf8buffer(cmd);
        utf8buffer.Append('\0');
        AppendUTF16toUTF8(cwd, utf8buffer);
        utf8buffer.Append('\0');

        // We used to set dwData to zero, when we didn't send the working dir.
        // Now we're using it as a version number.
        COPYDATASTRUCT cds = {
            1,
            utf8buffer.Length(),
            (void*) utf8buffer.get()
        };
        // Bring the already running Mozilla process to the foreground.
        // nsWindow will restore the window (if minimized) and raise it.
        ::SetForegroundWindow( mHandle );
        ::SendMessage( mHandle, WM_COPYDATA, 0, (LPARAM)&cds );
        return NS_OK;
    }

    // Window proc.
    static LRESULT CALLBACK WindowProc( HWND msgWindow, UINT msg, WPARAM wp, LPARAM lp ) {
        if ( msg == WM_COPYDATA ) {
            if (!nsNativeAppSupportWin::mCanHandleRequests)
                return FALSE;

            // This is an incoming request.
            COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lp;
#if MOZ_DEBUG_DDE
            printf( "Incoming request: %s\n", (const char*)cds->lpData );
#endif
            nsCOMPtr<nsILocalFile> workingDir;

            if (1 >= cds->dwData) {
                char* wdpath = (char*) cds->lpData;
                // skip the command line, and get the working dir of the
                // other process, which is after the first null char
                while (*wdpath)
                    ++wdpath;

                ++wdpath;

#ifdef MOZ_DEBUG_DDE
                printf( "Working dir: %s\n", wdpath);
#endif

                NS_NewLocalFile(NS_ConvertUTF8toUTF16(wdpath),
                                false,
                                getter_AddRefs(workingDir));
            }
            (void)nsNativeAppSupportWin::HandleCommandLine((char*)cds->lpData, workingDir, nsICommandLine::STATE_REMOTE_AUTO);

            // Get current window and return its window handle.
            nsCOMPtr<nsIDOMWindow> win;
            GetMostRecentWindow( 0, getter_AddRefs( win ) );
            return win ? (LRESULT)hwndForDOMWindow( win ) : 0;
        }
        return DefWindowProc( msgWindow, msg, wp, lp );
    }

private:
    HWND mHandle;
}; // struct MessageWindow

/* Start: Tries to find the "message window" to determine if it
 *        exists.  If so, then Mozilla is already running.  In that
 *        case, we use the handle to the "message" window and send
 *        a request corresponding to this process's command line
 *        options.
 *
 *        If not, then this is the first instance of Mozilla.  In
 *        that case, we create and set up the message window.
 *
 *        The checking for existence of the message window must
 *        be protected by use of a mutex semaphore.
 */
NS_IMETHODIMP
nsNativeAppSupportWin::Start( bool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance == 0, NS_ERROR_NOT_INITIALIZED );
    NS_ENSURE_STATE( gAppData );

    if (getenv("MOZ_NO_REMOTE"))
    {
        *aResult = true;
        return NS_OK;
    }

    nsresult rv = NS_ERROR_FAILURE;
    *aResult = false;

    // Grab mutex first.

    // Build mutex name from app name.
    ::_snwprintf(mMutexName, sizeof mMutexName / sizeof(PRUnichar), L"%s%s%s", 
                 MOZ_MUTEX_NAMESPACE,
                 NS_ConvertUTF8toUTF16(gAppData->name).get(),
                 MOZ_STARTUP_MUTEX_NAME );
    Mutex startupLock = Mutex( mMutexName );

    NS_ENSURE_TRUE( startupLock.Lock( MOZ_DDE_START_TIMEOUT ), NS_ERROR_FAILURE );

    // Search for existing message window.
    MessageWindow msgWindow;
    if ( (HWND)msgWindow ) {
        // We are a client process.  Pass request to message window.
        rv = msgWindow.SendRequest();
    } else {
        // We will be server.
        rv = msgWindow.Create();
        if ( NS_SUCCEEDED( rv ) ) {
            // Start up DDE server.
            this->StartDDE();
            // Tell caller to spin message loop.
            *aResult = true;
        }
    }

    startupLock.Unlock();

    return rv;
}

bool
nsNativeAppSupportWin::InitTopicStrings() {
    for ( int i = 0; i < topicCount; i++ ) {
        if ( !( mTopics[ i ] = DdeCreateStringHandleA( mInstance, const_cast<char *>(topicNames[ i ]), CP_WINANSI ) ) ) {
            return false;
        }
    }
    return true;
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
    NS_ENSURE_TRUE( ( mApplication = DdeCreateStringHandleA( mInstance, (char*) gAppData->name, CP_WINANSI ) ) && InitTopicStrings(),
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
nsNativeAppSupportWin::Stop( bool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_OK;
    *aResult = true;

    Mutex ddeLock( mMutexName );

    if ( ddeLock.Lock( MOZ_DDE_STOP_TIMEOUT ) ) {
        if ( mConversations == 0 ) {
            this->Quit();
        } else {
            *aResult = false;
        }

        ddeLock.Unlock();
    }
    else {
        // No DDE application name specified, but that's OK.  Just
        // forge ahead.
        *aResult = true;
    }

    return rv;
}

NS_IMETHODIMP
nsNativeAppSupportWin::Observe(nsISupports* aSubject, const char* aTopic,
                               const PRUnichar* aData)
{
    if (strcmp(aTopic, "quit-application") == 0) {
        Quit();
    } else {
        NS_ERROR("Unexpected observer topic.");
    }

    return NS_OK;
}

// Terminate DDE regardless.
NS_IMETHODIMP
nsNativeAppSupportWin::Quit() {
    // If another process wants to look for the message window, they need
    // to wait to hold the lock, in which case they will not find the
    // window as we will destroy ours under our lock.
    // When the mutex goes off the stack, it is unlocked via destructor.
    Mutex mutexLock(mMutexName);
    NS_ENSURE_TRUE(mutexLock.Lock(MOZ_DDE_START_TIMEOUT), NS_ERROR_FAILURE);

    // If we've got a message window to receive IPC or new window requests,
    // get rid of it as we are shutting down.
    // Note:  Destroy calls DestroyWindow, which will only work on a window
    //  created by the same thread.
    MessageWindow mw;
    mw.Destroy();

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
#if MOZ_DEBUG_DDE
    printf( "DDE server stopped\n" );
#endif
    }

    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportWin::Enable()
{
    mCanHandleRequests = true;

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->AddObserver(this, "quit-application", false);
    } else {
        NS_ERROR("No observer service?");
    }

    return NS_OK;
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


// Utility function to escape double-quotes within a string.
static void escapeQuotes( nsAString &aString ) {
    PRInt32 offset = -1;
    while( 1 ) {
       // Find next '"'.
       offset = aString.FindChar( '"', ++offset );
       if ( offset == kNotFound ) {
           // No more quotes, exit.
           break;
       } else {
           // Insert back-slash ahead of the '"'.
           aString.Insert( PRUnichar('\\'), offset );
           // Increment offset because we just inserted a slash
           offset++;
       }
    }
    return;
}

HDDEDATA CALLBACK
nsNativeAppSupportWin::HandleDDENotification( UINT uType,       // transaction type
                                              UINT uFmt,        // clipboard data format
                                              HCONV hconv,      // handle to the conversation
                                              HSZ hsz1,         // handle to a string
                                              HSZ hsz2,         // handle to a string
                                              HDDEDATA hdata,   // handle to a global memory object
                                              ULONG_PTR dwData1,    // transaction-specific data
                                              ULONG_PTR dwData2 ) { // transaction-specific data

    if (!mCanHandleRequests)
        return 0;


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

                    // Get the URL from the first argument in the command.
                    nsAutoString url;
                    ParseDDEArg(hsz2, 0, url);

                    // Read the 3rd argument in the command to determine if a
                    // new window is to be used.
                    nsAutoString windowID;
                    ParseDDEArg(hsz2, 2, windowID);
                    // "" means to open the URL in a new window.
                    if ( windowID.IsEmpty() ) {
                        url.Insert(NS_LITERAL_STRING("mozilla -new-window "), 0);
                    }
                    else {
                        url.Insert(NS_LITERAL_STRING("mozilla -url "), 0);
                    }

#if MOZ_DEBUG_DDE
                    printf( "Handling dde XTYP_REQUEST request: [%s]...\n", NS_ConvertUTF16toUTF8(url).get() );
#endif
                    // Now handle it.
                    HandleCommandLine(NS_ConvertUTF16toUTF8(url).get(), nsnull, nsICommandLine::STATE_REMOTE_EXPLICIT);

                    // Return pseudo window ID.
                    result = CreateDDEData( 1 );
                    break;
                }
                case topicGetWindowInfo: {
                    // This topic has to get the current URL, get the current
                    // page title and then format the output into the DDE
                    // return string.  The return value is "URL","Page Title",
                    // "Window ID" however the window ID is not used for this
                    // command, therefore it is returned as a null string

                    // This isn't really a loop.  We just use "break"
                    // statements to bypass the remaining steps when
                    // something goes wrong.
                    do {
                        // Get most recently used Nav window.
                        nsCOMPtr<nsIDOMWindow> navWin;
                        GetMostRecentWindow( NS_LITERAL_STRING( "navigator:browser" ).get(),
                                             getter_AddRefs( navWin ) );
                        if ( !navWin ) {
                            // There is not a window open
                            break;
                        }
                        // Get content window.
                        nsCOMPtr<nsIDOMWindow> content;
                        navWin->GetContent( getter_AddRefs( content ) );
                        if ( !content ) {
                            break;
                        }
                        // Convert that to internal interface.
                        nsCOMPtr<nsPIDOMWindow> internalContent( do_QueryInterface( content ) );
                        if ( !internalContent ) {
                            break;
                        }
                        // Get location.
                        nsCOMPtr<nsIDOMLocation> location;
                        internalContent->GetLocation( getter_AddRefs( location ) );
                        if ( !location ) {
                            break;
                        }
                        // Get href for URL.
                        nsAutoString url;
                        if ( NS_FAILED( location->GetHref( url ) ) ) {
                            break;
                        }
                        // Escape any double-quotes.
                        escapeQuotes( url );

                        // Now for the title...

                        // Get the base window from the doc shell...
                        nsCOMPtr<nsIBaseWindow> baseWindow =
                          do_QueryInterface( internalContent->GetDocShell() );
                        if ( !baseWindow ) {
                            break;
                        }
                        // And from the base window we can get the title.
                        nsXPIDLString title;
                        if(!baseWindow) {
                            break;
                        }
                        baseWindow->GetTitle(getter_Copies(title));
                        // Escape any double-quotes in the title.
                        escapeQuotes( title );

                        // Use a string buffer for the output data, first
                        // save a quote.
                        nsCAutoString   outpt( NS_LITERAL_CSTRING("\"") );
                        // Now copy the URL converting the Unicode string
                        // to a single-byte ASCII string
                        nsCAutoString tmpNativeStr;
                        NS_CopyUnicodeToNative( url, tmpNativeStr );
                        outpt.Append( tmpNativeStr );
                        // Add the "," used to separate the URL and the page
                        // title
                        outpt.Append( NS_LITERAL_CSTRING("\",\"") );
                        // Now copy the current page title to the return string
                        NS_CopyUnicodeToNative( title, tmpNativeStr );
                        outpt.Append( tmpNativeStr );
                        // Fill out the return string with the remainin ",""
                        outpt.Append( NS_LITERAL_CSTRING( "\",\"\"" ));

                        // Create a DDE handle to a char string for the data
                        // being returned, this copies and creates a "shared"
                        // copy of the DDE response until the calling APP
                        // reads it and says it can be freed.
                        result = CreateDDEData( (LPBYTE)(const char*)outpt.get(),
                                                outpt.Length() + 1 );
#if MOZ_DEBUG_DDE
                        printf( "WWW_GetWindowInfo->%s\n", outpt.get() );
#endif
                    } while ( false );
                    break;
                }
                case topicActivate: {
                    // Activate a Nav window...
                    nsAutoString windowID;
                    ParseDDEArg(hsz2, 0, windowID);
                    // 4294967295 is decimal for 0xFFFFFFFF which is also a
                    //   correct value to do that Activate last window stuff
                    if ( windowID.EqualsLiteral( "-1" ) ||
                         windowID.EqualsLiteral( "4294967295" ) ) {
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
                    result = CreateDDEData( false );
                    break;
                }
                case topicUnRegisterViewer: {
                    // Unregister new viewer (not implemented).
                    result = CreateDDEData( false );
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

            nsAutoString url;
            ParseDDEArg((const WCHAR*) request, 0, url);

            // Read the 3rd argument in the command to determine if a
            // new window is to be used.
            nsAutoString windowID;
            ParseDDEArg((const WCHAR*) request, 2, windowID);

            // "" means to open the URL in a new window.
            if ( windowID.IsEmpty() ) {
                url.Insert(NS_LITERAL_STRING("mozilla -new-window "), 0);
            }
            else {
                url.Insert(NS_LITERAL_STRING("mozilla -url "), 0);
            }
#if MOZ_DEBUG_DDE
            printf( "Handling dde XTYP_REQUEST request: [%s]...\n", NS_ConvertUTF16toUTF8(url).get() );
#endif
            // Now handle it.
            HandleCommandLine(NS_ConvertUTF16toUTF8(url).get(), nsnull, nsICommandLine::STATE_REMOTE_EXPLICIT);

            // Release the data.
            DdeUnaccessData( hdata );
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

// Utility function to advance to end of quoted string.
// p+offset must point to the comma preceding the arg on entry.
// On return, p+result points to the closing '"' (or end of the string
// if the closing '"' is missing) if the arg is quoted.  If the arg
// is not quoted, then p+result will point to the first character
// of the arg.
static PRInt32 advanceToEndOfQuotedArg( const WCHAR *p, PRInt32 offset, PRInt32 len ) {
    // Check whether the current arg is quoted.
    if ( p[++offset] == '"' ) {
        // Advance past the closing quote.
        while ( offset < len && p[++offset] != '"' ) {
            // If the current character is a backslash, then the
            // next character can't be a *real* '"', so skip it.
            if ( p[offset] == '\\' ) {
                offset++;
            }
        }
    }
    return offset;
}

void nsNativeAppSupportWin::ParseDDEArg( const WCHAR* args, int index, nsString& aString) {
    if ( args ) {
        nsDependentString temp(args);

        // offset points to the comma preceding the desired arg.
        PRInt32 offset = -1;
        // Skip commas till we get to the arg we want.
        while( index-- ) {
            // If this arg is quoted, then go to closing quote.
            offset = advanceToEndOfQuotedArg( args, offset, temp.Length());
            // Find next comma.
            offset = temp.FindChar( ',', offset );
            if ( offset == kNotFound ) {
                // No more commas, give up.
                aString = args;
                return;
            }
        }
        // The desired argument starts just past the preceding comma,
        // which offset points to, and extends until the following
        // comma (or the end of the string).
        //
        // Since the argument might be enclosed in quotes, we need to
        // deal with that before searching for the terminating comma.
        // We advance offset so it ends up pointing to the start of
        // the argument we want.
        PRInt32 end = advanceToEndOfQuotedArg( args, offset++, temp.Length() );
        // Find next comma (or end of string).
        end = temp.FindChar( ',', end );
        if ( end == kNotFound ) {
            // Arg is the rest of the string.
            end = temp.Length();
        }
        // Extract result.
        aString.Assign( args + offset, end - offset );
    }
    return;
}

// Utility to parse out argument from a DDE item string.
void nsNativeAppSupportWin::ParseDDEArg( HSZ args, int index, nsString& aString) {
    DWORD argLen = DdeQueryStringW( mInstance, args, NULL, 0, CP_WINUNICODE );
    // there wasn't any string, so return empty string
    if ( !argLen ) return;
    nsAutoString temp;
    // Ensure result's buffer is sufficiently big.
    temp.SetLength( argLen );
    // Now get the string contents.
    DdeQueryString( mInstance, args, temp.BeginWriting(), temp.Length(), CP_WINUNICODE );
    // Parse out the given arg.
    ParseDDEArg(temp.get(), index, aString);
    return;
}

HDDEDATA nsNativeAppSupportWin::CreateDDEData( DWORD value ) {
    return CreateDDEData( (LPBYTE)&value, sizeof value );
}

HDDEDATA nsNativeAppSupportWin::CreateDDEData( LPBYTE value, DWORD len ) {
    HDDEDATA result = DdeCreateDataHandle( mInstance,
                                           value,
                                           len,
                                           0,
                                           mApplication,
                                           CF_TEXT,
                                           0 );
    return result;
}

void nsNativeAppSupportWin::ActivateLastWindow() {
    nsCOMPtr<nsIDOMWindow> navWin;
    GetMostRecentWindow( NS_LITERAL_STRING("navigator:browser").get(), getter_AddRefs( navWin ) );
    if ( navWin ) {
        // Activate that window.
        activateWindow( navWin );
    } else {
        // Need to create a Navigator window, then.
        OpenBrowserWindow();
    }
}

void
nsNativeAppSupportWin::HandleCommandLine(const char* aCmdLineString,
                                         nsIFile* aWorkingDir,
                                         PRUint32 aState)
{
    nsresult rv;

    int justCounting = 1;
    char **argv = 0;
    // Flags, etc.
    int init = 1;
    int between, quoted, bSlashCount;
    int argc;
    const char *p;
    nsCAutoString arg;

    nsCOMPtr<nsICommandLineRunner> cmdLine
        (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
    if (!cmdLine) {
        NS_ERROR("Couldn't create command line!");
        return;
    }

    // Parse command line args according to MS spec
    // (see "Parsing C++ Command-Line Arguments" at
    // http://msdn.microsoft.com/library/devprods/vs6/visualc/vclang/_pluslang_parsing_c.2b2b_.command.2d.line_arguments.htm).
    // We loop if we've not finished the second pass through.
    while ( 1 ) {
        // Initialize if required.
        if ( init ) {
            p = aCmdLineString;
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

    rv = cmdLine->Init(argc, argv, aWorkingDir, aState);

    // Cleanup.
    while ( argc ) {
        delete [] argv[ --argc ];
    }
    delete [] argv;

    if (NS_FAILED(rv)) {
        NS_ERROR("Error initializing command line.");
        return;
    }

    cmdLine->Run();
}

nsresult
nsNativeAppSupportWin::OpenWindow( const char*urlstr, const char *args ) {

  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsISupportsCString> sarg(do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
  if (sarg)
    sarg->SetData(nsDependentCString(args));

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

HWND hwndForDOMWindow( nsISupports *window ) {
    nsCOMPtr<nsPIDOMWindow> pidomwindow( do_QueryInterface(window) );
    if ( !pidomwindow ) {
        return 0;
    }

    nsCOMPtr<nsIBaseWindow> ppBaseWindow =
        do_QueryInterface( pidomwindow->GetDocShell() );
    if ( !ppBaseWindow ) {
        return 0;
    }

    nsCOMPtr<nsIWidget> ppWidget;
    ppBaseWindow->GetMainWidget( getter_AddRefs( ppWidget ) );

    return (HWND)( ppWidget->GetNativeData( NS_NATIVE_WIDGET ) );
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
  if (mContext) // only once
    return NS_ERROR_FAILURE;

  mService = do_GetService(sJSStackContractID);
  if (mService) {
    JSContext* cx = mService->GetSafeJSContext();
    if (cx && NS_SUCCEEDED(mService->Push(cx))) {
      // Save cx in mContext to indicate need to pop.
      mContext = cx;
    }
  }
  return mContext ? NS_OK : NS_ERROR_FAILURE;
}


nsresult
nsNativeAppSupportWin::OpenBrowserWindow()
{
    nsresult rv = NS_OK;

    // Open the argument URL in the most recently used Navigator window.
    // If there is no Nav window, open a new one.

    // If at all possible, hand the request off to the most recent
    // browser window.

    nsCOMPtr<nsIDOMWindow> navWin;
    GetMostRecentWindow( NS_LITERAL_STRING( "navigator:browser" ).get(), getter_AddRefs( navWin ) );

    // This isn't really a loop.  We just use "break" statements to fall
    // out to the OpenWindow call when things go awry.
    do {
        // If caller requires a new window, then don't use an existing one.
        if ( !navWin ) {
            // Have to open a new one.
            break;
        }

        nsCOMPtr<nsIBrowserDOMWindow> bwin;
        { // scope a bunch of temporary cruft used to generate bwin
          nsCOMPtr<nsIWebNavigation> navNav( do_GetInterface( navWin ) );
          nsCOMPtr<nsIDocShellTreeItem> navItem( do_QueryInterface( navNav ) );
          if ( navItem ) {
            nsCOMPtr<nsIDocShellTreeItem> rootItem;
            navItem->GetRootTreeItem( getter_AddRefs( rootItem ) );
            nsCOMPtr<nsIDOMWindow> rootWin( do_GetInterface( rootItem ) );
            nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(rootWin));
            if ( chromeWin )
              chromeWin->GetBrowserDOMWindow( getter_AddRefs ( bwin ) );
          }
        }
        if ( bwin ) {
          nsCOMPtr<nsIURI> uri;
          NS_NewURI( getter_AddRefs( uri ), NS_LITERAL_CSTRING("about:blank"), 0, 0 );
          if ( uri ) {
            nsCOMPtr<nsIDOMWindow> container;
            rv = bwin->OpenURI( uri, 0,
                                nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW,
                                nsIBrowserDOMWindow::OPEN_EXTERNAL,
                                getter_AddRefs( container ) );
            if ( NS_SUCCEEDED( rv ) )
              return NS_OK;
          }
        }

        NS_ERROR("failed to hand off external URL to extant window");
    } while ( false );

    // open a new window if caller requested it or if anything above failed

    char* argv[] = { 0 };
    nsCOMPtr<nsICommandLineRunner> cmdLine
        (do_CreateInstance("@mozilla.org/toolkit/command-line;1"));
    NS_ENSURE_TRUE(cmdLine, NS_ERROR_FAILURE);

    rv = cmdLine->Init(0, argv, nsnull, nsICommandLine::STATE_REMOTE_EXPLICIT);
    NS_ENSURE_SUCCESS(rv, rv);

    return cmdLine->Run();
}

