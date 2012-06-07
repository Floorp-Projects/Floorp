/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_OS2_HIGH_MEMORY
// os2safe.h has to be included before os2.h, needed for high mem
#include <os2safe.h>
#endif

#define INCL_PM
#define INCL_GPI
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "nsNativeAppSupportBase.h"
#include "nsNativeAppSupportOS2.h"
#include "nsAppRunner.h"
#include "nsXULAppAPI.h"
#include "nsString.h"
#include "nsIBrowserDOMWindow.h"
#include "nsICommandLineRunner.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsArray.h"
#include "nsIWindowWatcher.h"
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

// These are needed to load a URL in a browser window.
#include "nsIDOMLocation.h"
#include "nsIJSContextStack.h"
#include "nsIWebNavigation.h"
#include "nsIWindowMediator.h"

#include <stdlib.h>
#include "prprf.h"

#define kMailtoUrlScheme "mailto:"

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


#ifdef DEBUG_law
#undef MOZ_DEBUG_DDE
#define MOZ_DEBUG_DDE 1
#endif

// Simple Win32 mutex wrapper.
struct Mutex {
    Mutex( const char *name )
        : mName( name ),
          mHandle( 0 ),
          mState( 0xFFFF ) {
        /* OS/2 named semaphores must begin with "\\SEM32\\" to be valid */
        mName.Insert("\\SEM32\\", 0);
        APIRET rc = DosCreateMutexSem(mName.get(), &mHandle, 0, FALSE);
        if (rc != NO_ERROR) {
#if MOZ_DEBUG_DDE
            printf("CreateMutex error = 0x%08X\n", (int)rc);
#endif
        }
    }
    ~Mutex() {
        if ( mHandle ) {
            // Make sure we release it if we own it.
            Unlock();

            APIRET rc = DosCloseMutexSem(mHandle);
            if (rc != NO_ERROR) {
#if MOZ_DEBUG_DDE
                printf("CloseHandle error = 0x%08X\n", (int)rc);
#endif
            }
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
            return (mState == NO_ERROR);
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
            mState = 0xFFFF;
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

class nsNativeAppSupportOS2 : public nsNativeAppSupportBase,
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
    static HDDEDATA APIENTRY HandleDDENotification( ULONG    idInst,
                                                    USHORT   uType,
                                                    USHORT   uFmt,
                                                    HCONV    hconv,
                                                    HSZ      hsz1,
                                                    HSZ      hsz2,
                                                    HDDEDATA hdata,
                                                    ULONG    dwData1,
                                                    ULONG    dwData2 );
    static void HandleCommandLine(const char* aCmdLineString, nsIFile* aWorkingDir, PRUint32 aState);
    static void ParseDDEArg( HSZ args, int index, nsCString& string);
    static void ParseDDEArg( const char* args, int index, nsCString& aString);
    static void ActivateLastWindow();
    static HDDEDATA CreateDDEData( DWORD value );
    static HDDEDATA CreateDDEData( LPBYTE value, DWORD len );
    static bool     InitTopicStrings();
    static int      FindTopic( HSZ topic );
    static nsresult OpenWindow( const char *urlstr, const char *args );
    static nsresult OpenBrowserWindow();

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
    static char mMutexName[];
    static bool mUseDDE;
    friend struct MessageWindow;
}; // nsNativeAppSupportOS2

NS_INTERFACE_MAP_BEGIN(nsNativeAppSupportOS2)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsNativeAppSupportBase)

NS_IMPL_ADDREF_INHERITED(nsNativeAppSupportOS2, nsNativeAppSupportBase)
NS_IMPL_RELEASE_INHERITED(nsNativeAppSupportOS2, nsNativeAppSupportBase)

void
nsNativeAppSupportOS2::CheckConsole() {
    return;
}


// Create and return an instance of class nsNativeAppSupportOS2.
nsresult
NS_CreateNativeAppSupport( nsINativeAppSupport **aResult ) {
    nsNativeAppSupportOS2 *pNative = new nsNativeAppSupportOS2;
    if (!pNative) return NS_ERROR_OUT_OF_MEMORY;

    // Check for dynamic console creation request.
    pNative->CheckConsole();

    *aResult = pNative;
    NS_ADDREF( *aResult );

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
                                    "WWW_UnRegisterViewer",
                                    "WWW_GetWindowInfo" };

// Static member definitions.
int   nsNativeAppSupportOS2::mConversations = 0;
HSZ   nsNativeAppSupportOS2::mApplication   = 0;
HSZ   nsNativeAppSupportOS2::mTopics[nsNativeAppSupportOS2::topicCount] = { 0 };
DWORD nsNativeAppSupportOS2::mInstance      = 0;
bool nsNativeAppSupportOS2::mCanHandleRequests   = false;

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

    rc = DosLoadModule( NULL, 0, "PMDDEML", &hmodDDEML );
    if( rc == NO_ERROR )
    {
       int i;
       /* all of this had better work.  Get ready to be a success! */
       bRC = TRUE;
       for( i=0; ddemlfnTable[i].ord != 0; i++ )
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

char nsNativeAppSupportOS2::mMutexName[ 128 ] = { 0 };


// Message window encapsulation.
struct MessageWindow {
    // ctor/dtor are simplistic
    MessageWindow() {
        // Try to find window.
        HATOMTBL  hatomtbl;
        HENUM     henum;
        HWND      hwndNext;
        char      classname[CCHMAXPATH];

        mHandle = NULLHANDLE;

        hatomtbl = WinQuerySystemAtomTable();
        mMsgWindowAtom = WinFindAtom(hatomtbl, className());
        if (mMsgWindowAtom == 0)
        {
          // If there is not atom in the system table for this class name, then
          // we can assume that the app is not currently running.
          mMsgWindowAtom = WinAddAtom(hatomtbl, className());
        } else {
          // Found an existing atom for this class name.  Cycle through existing
          // windows and see if one with our window id and class name already 
          // exists
          henum = WinBeginEnumWindows(HWND_OBJECT);
          while ((hwndNext = WinGetNextWindow(henum)) != NULLHANDLE)
          {
            if (WinQueryWindowUShort(hwndNext, QWS_ID) == (USHORT)mMsgWindowAtom)
            {
              WinQueryClassName(hwndNext, CCHMAXPATH, classname);
              if (strcmp(classname, className()) == 0)
              {
                mHandle = hwndNext;
                break;
              }
            }
          }
        }
    }

    HWND getHWND() {
        return mHandle;
    }

    // Class name: appName + "MessageWindow"
    static const char *className() {
        static char classNameBuffer[128];
        static char *mClassName = 0;
        if ( !mClassName ) { 
            sprintf( classNameBuffer,
                         "%s%s",
                         gAppData->name,
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
                                   (USHORT)mMsgWindowAtom, // id
                                   NULL,        // pCtlData
                                   NULL );      // pres params

#if MOZ_DEBUG_DDE
        printf( "Message window = 0x%08X\n", (int)mHandle );
#endif

        return NS_OK;
    }

    // Destory:  Get rid of window and reset mHandle.
    NS_IMETHOD Destroy() {
        nsresult retval = NS_OK;

        if ( mHandle ) {
           HATOMTBL hatomtbl = WinQuerySystemAtomTable();
           WinDeleteAtom(hatomtbl, mMsgWindowAtom);
           
            // DestroyWindow can only destroy windows created from
            //  the same thread.
            BOOL desRes = WinDestroyWindow( mHandle );
            if ( FALSE != desRes ) {
                mHandle = NULL;
            }
            else {
                retval = NS_ERROR_FAILURE;
            }
        }

        return retval;
    }

    // SendRequest: Construct a data buffer <commandline>\0<workingdir>\0,
    // then pass the string via WM_COPYDATA to the message window.

    NS_IMETHOD SendRequest( const char *cmd )
    {
    /* Nothing like WM_COPYDATA in OS/2, where the OS allows pointers to be
     * passed to a different process and automatically accessible by that
     * process.  So we have to create shared mem on our side and then the
     * process that gets the WM_COPYDATA message has to do a 
     * DosGetSharedMem on this pointer to be able to access the data
    */

        COPYDATASTRUCT *pcds;
        PVOID pvData = NULL;

        if (!cmd)
            return NS_ERROR_FAILURE;

        ULONG ulSize = sizeof(COPYDATASTRUCT)+strlen(cmd)+1+CCHMAXPATH;
#ifdef MOZ_OS2_HIGH_MEMORY
        APIRET rc = DosAllocSharedMem( &pvData, NULL, ulSize,
                                       PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_GETTABLE | OBJ_ANY);
        if (rc != NO_ERROR) { // Did the kernel handle OBJ_ANY?
            // Try again without OBJ_ANY and if the first failure was not caused
            // by OBJ_ANY then we will get the same failure, else we have taken
            // care of pre-FP13 systems where the kernel couldn't handle it.
            rc = DosAllocSharedMem( &pvData, NULL, ulSize,
                                    PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_GETTABLE);
            if (rc != NO_ERROR) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
#else
        if (DosAllocSharedMem( &pvData, NULL, ulSize,
                               PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_GETTABLE | OBJ_ANY))
            return NS_ERROR_OUT_OF_MEMORY;
#endif

        // We used to set dwData to zero, when we didn't send the
        // working dir.  Now we're using it as a version number.
        // The commandline goes in the buffer space immediately after
        // the COPYDATASTRUCT;  the current directory follows that.
        pcds = (COPYDATASTRUCT *)(pvData);
        pcds->dwData = 1;

        char * ptr = &(pcds->chBuff);
        pcds->lpData = ptr;
        strcpy( ptr, cmd);
        pcds->cbData = strlen( ptr) + 1;
        ptr += pcds->cbData;

        if (DosQueryPathInfo( ".", FIL_QUERYFULLNAME, ptr, CCHMAXPATH)) {
            ptr[0] = '.';
            ptr[1] = '\0';
        }
        pcds->cbData += strlen( ptr) + 1;

        WinSendMsg( mHandle, WM_COPYDATA, 0, (MPARAM)pcds );
        DosFreeMem( pvData );

        return NS_OK;
    }

    // Window proc.
    static
    MRESULT EXPENTRY WindowProc( HWND msgWindow, ULONG msg, MPARAM wp, MPARAM lp )
    {
        // We have to return a FALSE from WM_CREATE
        if ( msg == WM_CREATE )
            return (MRESULT)FALSE;

        if ( msg != WM_COPYDATA ) 
            return (MRESULT)TRUE;

        // This is an incoming request.
        COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lp;
        DosGetSharedMem( (PVOID)cds, PAG_READ|PAG_WRITE );

        nsCOMPtr<nsIFile> workingDir;

        // a "1" or greater indicates that the other process's working
        // directory follows the commandline string - locate & convert it
        if (cds->dwData >= 1) {
            char* wdpath = strchr( (char*)cds->lpData, 0) + 1;
            NS_NewNativeLocalFile(nsDependentCString(wdpath),
                                  false, getter_AddRefs(workingDir));
        }

        nsNativeAppSupportOS2::HandleCommandLine((char*)cds->lpData,
                                workingDir, nsICommandLine::STATE_REMOTE_AUTO);

        return (MRESULT)TRUE;
    }

private:
    HWND mHandle;
    USHORT   mMsgWindowAtom;
}; // struct MessageWindow

bool nsNativeAppSupportOS2::mUseDDE = false;

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
nsNativeAppSupportOS2::Start( bool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance == 0, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_ERROR_FAILURE;
    *aResult = false;

    // see if DDE should be enabled AND remove OS/2-specific
    // options the app's commandline handler won't recognize
    // (-console was handled earlier by StartOS2App())
    for (int i = 1; i < gArgc; i++) {
        if (stricmp("-dde", gArgv[i]) == 0 ||
            stricmp("/dde", gArgv[i]) == 0)
            mUseDDE = true;
        else
            if (stricmp("-console", gArgv[i]) != 0 &&
                stricmp("/console", gArgv[i]) != 0)
                continue;

        for (int j = i; j < gArgc; j++)
            gArgv[j] = gArgv[j+1];
        gArgc--;
        i--;
    }

    // if this is a standalone instance, turn off DDE regardless of the
    // commandline, then skip out before we look for another instance
    if (getenv("MOZ_NO_REMOTE")) {
        mUseDDE = false;
        *aResult = true;
        return NS_OK;
    }

    // Grab mutex first.

    // Build mutex name from app name.
    PR_snprintf( mMutexName, sizeof mMutexName, "%s%s", gAppData->name, MOZ_STARTUP_MUTEX_NAME );
    Mutex startupLock = Mutex( mMutexName );

    NS_ENSURE_TRUE( startupLock.Lock( MOZ_DDE_START_TIMEOUT ), NS_ERROR_FAILURE );

    /* We need to have a message queue to do the MessageWindow stuff (for both
     * Create() and SendRequest()).  If we don't have one, make it now.
     * If we are going to end up going away right after ::Start() returns,
     * then make sure to clean up the message queue.
     */
    MQINFO mqinfo;
    HAB hab;
    HMQ hmqCurrent = WinQueryQueueInfo( HMQ_CURRENT, &mqinfo, 
                                        sizeof( MQINFO ) );
    if( !hmqCurrent ) {
        hab = WinInitialize( 0 );
        hmqCurrent = WinCreateMsgQueue( hab, 0 );
    }

    // Search for existing message window.
    MessageWindow msgWindow;
    if ( msgWindow.getHWND() ) {
        // We are a client process.  Pass request to message window.
        char *cmd = GetCommandLine();
        rv = msgWindow.SendRequest( cmd );
    } else {
        // We will be server.
        rv = msgWindow.Create();
        if ( NS_SUCCEEDED( rv ) ) {
            // Start up DDE server
            if (mUseDDE)
                this->StartDDE();
            // Tell caller to spin message loop.
            *aResult = true;
        }
    }

    startupLock.Unlock();

    if( *aResult == false )
    {
        /* This process isn't going to continue much longer.  Make sure that we
         * clean up the message queue
         */
        if (hmqCurrent)
           WinDestroyMsgQueue(hmqCurrent);
        if (hab)
           WinTerminate(hab);
    }

    return rv;
}

bool
nsNativeAppSupportOS2::InitTopicStrings() {
    for ( int i = 0; i < topicCount; i++ ) {
        if ( !( mTopics[ i ] = WinDdeCreateStringHandle( (PSZ)topicNames[ i ], CP_WINANSI ) ) ) {
            return false;
        }
    }
    return true;
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

    /* Get entrypoints for PMDDEML */
    BOOL bDDEML = SetupOS2ddeml();

    /* If we couldn't initialize DDEML, set mUSEDDE to false */
    /* so we don't do anything else DDE related */
    if (!bDDEML) {
       mUseDDE = false;
       return NS_OK;
    }

    // Initialize DDE.
    NS_ENSURE_TRUE( DDEERR_NO_ERROR == WinDdeInitialize( &mInstance,
                                                         nsNativeAppSupportOS2::HandleDDENotification,
                                                         APPCLASS_STANDARD,
                                                         0 ),
                    NS_ERROR_FAILURE );

    // Allocate DDE strings.
    NS_ENSURE_TRUE( ( mApplication = WinDdeCreateStringHandle( (char*) gAppData->name, CP_WINANSI ) ) && InitTopicStrings(),
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
nsNativeAppSupportOS2::Stop( bool *aResult ) {
    NS_ENSURE_ARG( aResult );
    NS_ENSURE_TRUE( mInstance, NS_ERROR_NOT_INITIALIZED );

    nsresult rv = NS_OK;
    *aResult = true;

    if (!mUseDDE) {
       return rv;
    }

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
nsNativeAppSupportOS2::Observe(nsISupports* aSubject, const char* aTopic,
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
nsNativeAppSupportOS2::Quit() {
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

NS_IMETHODIMP
nsNativeAppSupportOS2::Enable()
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
    DWORD len = WinDdeQueryString( hsz, NULL, NULL, CP_WINANSI );
    if ( len ) {
        char buffer[ 256 ];
        WinDdeQueryString( hsz, buffer, sizeof buffer, CP_WINANSI );
        result += buffer;
    }
    result += "]";
    return result;
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

                    // Get the URL from the first argument in the command.
                    nsCAutoString url;
                    ParseDDEArg(hsz2, 0, url);

                    // Read the 3rd argument in the command to determine if a
                    // new window is to be used.
                    nsCAutoString windowID;
                    ParseDDEArg(hsz2, 2, windowID);
                    // to open the URL in a new window, the old OS/2 code
                    // looks for "0" while the new Win32 code looks for "";
                    // not knowing which is correct, we'll look for both
                    if (windowID.Equals( "0" ) || windowID.Equals( "" ))
                        url.Insert("mozilla -new-window ", 0);
                    else
                        url.Insert("mozilla -url ", 0);

#if MOZ_DEBUG_DDE
                    printf( "Handling dde XTYP_REQUEST request: [%s]...\n", url.get() );
#endif
                    // Now handle it.
                    HandleCommandLine(url.get(), nsnull, nsICommandLine::STATE_REMOTE_EXPLICIT);
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
                        outpt.Append( NS_LossyConvertUTF16toASCII( url ) );
                        // Add the "," used to separate the URL and the page
                        // title
                        outpt.Append( NS_LITERAL_CSTRING("\",\"") );
                        // Now copy the current page title to the return string
                        outpt.Append( NS_LossyConvertUTF16toASCII( title.get() ));
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
                    nsCAutoString windowID;
                    ParseDDEArg(hsz2, 0, windowID);
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
            LPBYTE request = (LPBYTE)WinDdeAccessData( hdata, &bytes );
#if MOZ_DEBUG_DDE
            printf( "Handling dde request: [%s]...\n", (char*)request );
#endif
            nsCAutoString url;
            ParseDDEArg((const char*) request, 0, url);

            // Read the 3rd argument in the command to determine if a
            // new window is to be used.
            nsCAutoString windowID;
            ParseDDEArg((const char*) request, 2, windowID);

            // to open the URL in a new window, the old OS/2 code
            // looks for "0" while the new Win32 code looks for "";
            // not knowing which is correct, we'll look for both
            if (windowID.Equals( "0" ) || windowID.Equals( "" ))
                url.Insert("mozilla -new-window ", 0);
            else
                url.Insert("mozilla -url ", 0);

#if MOZ_DEBUG_DDE
            printf( "Handling dde XTYP_REQUEST request: [%s]...\n", url.get() );
#endif
            // Now handle it.
            HandleCommandLine(url.get(), nsnull, nsICommandLine::STATE_REMOTE_EXPLICIT);

            // Release the data.
//            DdeUnaccessData( hdata );
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


// Utility function to advance to end of quoted string.
// p+offset must point to the comma preceding the arg on entry.
// On return, p+result points to the closing '"' (or end of the string
// if the closing '"' is missing) if the arg is quoted.  If the arg
// is not quoted, then p+result will point to the first character
// of the arg.
static PRInt32 advanceToEndOfQuotedArg( const char *p, PRInt32 offset, PRInt32 len ) {
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

void nsNativeAppSupportOS2::ParseDDEArg( const char* args, int index, nsCString& aString) {
    if ( args ) {
        int argLen = strlen(args);
        nsDependentCString temp(args, argLen);

        // offset points to the comma preceding the desired arg.
        PRInt32 offset = -1;
        // Skip commas till we get to the arg we want.
        while( index-- ) {
            // If this arg is quoted, then go to closing quote.
            offset = advanceToEndOfQuotedArg( args, offset, argLen);
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
        PRInt32 end = advanceToEndOfQuotedArg( args, offset++, argLen );
        // Find next comma (or end of string).
        end = temp.FindChar( ',', end );
        if ( end == kNotFound ) {
            // Arg is the rest of the string.
            end = argLen;
        }
        // Extract result.
        aString.Assign( args + offset, end - offset );
    }
    return;
}

// Utility to parse out argument from a DDE item string.
void nsNativeAppSupportOS2::ParseDDEArg( HSZ args, int index, nsCString& aString) {
    DWORD argLen = WinDdeQueryString( args, NULL, NULL, CP_WINANSI );
    // there wasn't any string, so return empty string
    if ( !argLen ) return;
    nsCAutoString temp;
    // Ensure result's buffer is sufficiently big.
    temp.SetLength( argLen );
    // Now get the string contents.
    WinDdeQueryString( args, temp.BeginWriting(), temp.Length(), CP_WINANSI );
    // Parse out the given arg.
    ParseDDEArg(temp.get(), index, aString);
    return;
}

void nsNativeAppSupportOS2::ActivateLastWindow() {
    nsCOMPtr<nsIDOMWindow> navWin;
    GetMostRecentWindow( NS_LITERAL_STRING("navigator:browser").get(), getter_AddRefs( navWin ) );
    if ( navWin )
        // Activate that window.
        activateWindow( navWin );
    else
        // Need to create a Navigator window, then.
        OpenBrowserWindow();
}

HDDEDATA nsNativeAppSupportOS2::CreateDDEData( DWORD value ) {
    return CreateDDEData( (LPBYTE)&value, sizeof value );
}

HDDEDATA nsNativeAppSupportOS2::CreateDDEData( LPBYTE value, DWORD len ) {
    HDDEDATA result = WinDdeCreateDataHandle( value,
                                              len,
                                              0,
                                              mApplication,
                                              CF_TEXT,
                                              0 );
    return result;
}


void
nsNativeAppSupportOS2::HandleCommandLine(const char* aCmdLineString,
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

            // if the argument we are parsing is a mailto url then all
            // of the remaining command line data needs to be part of
            // the mailto url even if it has spaces. See Bug #231032
            nsDependentCString mailtoUrlScheme (kMailtoUrlScheme);

            if ( *p == 0 || ( !quoted && isspace( *p ) && !StringBeginsWith(arg, mailtoUrlScheme, nsCaseInsensitiveCStringComparator()) ) ) {
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

    // if these OS/2-specific flags aren't removed,
    // any URL following them will be ignored
    bool found;
    cmdLine->HandleFlag(NS_LITERAL_STRING("console"), false, &found);
    cmdLine->HandleFlag(NS_LITERAL_STRING("dde"), false, &found);

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
nsNativeAppSupportOS2::OpenWindow( const char*urlstr, const char *args ) {

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


// As of Jan, 2005, most of the code in this method is pointless and
// will never be used.  It is only called by ActivateLastWindow() and
// only when there is no existing window.  Consequently, the first test
// below [if ( !navWin )] will always fail, bypassing everything in the
// dummy do..while() loop.  Given that this file is largely a port of
// the Win32 version, it is left in place to maintain concurrency.

nsresult
nsNativeAppSupportOS2::OpenBrowserWindow()
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

// This is a public function called by nsAppRunner.cpp.  Its primary
// purpose is to determine if any commandline options require a VIO
// ("console") window.  If so and one isn't present, it will restart
// the app in a VIO session.  It is intended to be called as early as
// possible during startup and before any other commandline processing.
// It returns TRUE if the current instance should continue running and
// FALSE if it should terminate upon this function's return.

bool       StartOS2App(int aArgc, char **aArgv)
{
  bool      rv = true;
  PPIB      ppib;
  PTIB      ptib;

  DosGetInfoBlocks(&ptib, &ppib);

  // if this isn't a PM session, reset the session type to enable use
  // of PM functions;  if it is PM, see if a VIO session is required
  if (ppib->pib_ultype != SSF_TYPE_PM)
    ppib->pib_ultype = SSF_TYPE_PM;
  else {
    for (int i = 1; i < aArgc; i++ ) {
      char *arg = aArgv[i];
      if (*arg != '-' && *arg != '/')
        continue;
      arg++;
      if (stricmp("?", arg) == 0 ||
        stricmp("h", arg) == 0 ||
        stricmp("v", arg) == 0 ||
        stricmp("help", arg) == 0 ||
        stricmp("version", arg) == 0 ||
        stricmp("console", arg) == 0) {
        rv = false;
        break;
      }
    }
  }

  // if the session type is OK, increase the number of 
  // file handles available to the app, then exit
  if (rv) {
    ULONG    ulMaxFH = 0;
    LONG     ulReqCount = 0;

    DosSetRelMaxFH(&ulReqCount, &ulMaxFH);
    if (ulMaxFH < 256)
      DosSetMaxFH(256);

    return rv;
  }

  // the app has to be restarted in a VIO session
  char        szErrObj[64] = "";
  STARTDATA   x;

  memset(&x, 0, sizeof(x));
  x.Length = sizeof(x);
  x.PgmTitle = (PSZ)gAppData->name;
  x.InheritOpt = SSF_INHERTOPT_PARENT;
  x.SessionType = SSF_TYPE_WINDOWABLEVIO;
  x.PgmControl = SSF_CONTROL_NOAUTOCLOSE;
  x.ObjectBuffer = szErrObj;
  x.ObjectBuffLen = sizeof(szErrObj);

  // the f/q exename is the string preceding ppib->pib_pchcmd;
  // the original commandline is the string following it
  char * ptr = ppib->pib_pchcmd - 2;
  while (*ptr)
    ptr--;
  x.PgmName = ptr + 1;
  x.PgmInputs = strchr(ppib->pib_pchcmd, 0) + 1;

  // restart the app;  if this session is in the background, trying
  // to start in the foreground will produce an error, but the app
  // will still start; if DosStartSession has a real failure, forget
  // the console and let the current instance keep running
  ULONG ulSession;
  PID   pid;
  ULONG rc = DosStartSession(&x, &ulSession, &pid);
  if (rc && rc != ERROR_SMG_START_IN_BACKGROUND)
    rv = true;

  return rv;
}

