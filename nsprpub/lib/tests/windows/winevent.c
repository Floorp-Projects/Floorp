/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
** File:        winevent.c
** Description: Test functions in plevent.c using Windows
**
** The winevent test exercises the PLEvent library in a maner
** similar to how the Mozilla (or NGLayout) Client will use
** it in a Windows environment.
**
** This test is based on ideas taken from Charles Petzold's
** book "Programming Windows 3.1". License to use is in the
** book. It has been ported to Win32.
**
** Operation:
** The initialization is a standard Windows GUI application
** setup. When the main window receives its WM_CREATE
** message, a child window is created, a edit control is
** instantiated in that window.
**
** A thread is created; this thread runs in the function:
** TimerThread(). The new thread sends a message every second
** via the PL_PostEvent() function. The event handler
** HandlePadEvent() sends a windows message to the edit
** control window; these messages are WM_CHAR messages that
** cause the edit control to place a single '.' character in
** the edit control. 
**
** After a deterministic number of '.' characters, the
** TimerThread() function is notified via a global variable
** that it's quitting time.
** 
** TimerThread() callse TestEvents(), an external function
** that tests additional function of PLEvent.
**
*/

#include "nspr.h"
#include "plevent.h"

#include <windows.h>
#include <commdlg.h>

#define ID_EDIT     1

/* 
** Declarations for NSPR customization
** 
*/
typedef struct PadEvent
{
    PLEvent plEvent;
    int     unused;    
} PadEvent;

static void PR_CALLBACK TimerThread( void *arg);
static void PR_CALLBACK HandlePadEvent( PadEvent *padEvent );
static void PR_CALLBACK DestroyPadEvent( PadEvent *padevent );

static PRThread *tThread;
static PLEventQueue *padQueue; 
static long ThreadSleepTime = 1000; /* in milli-seconds */
static long timerCount = 0;
static HWND hDlgModeless ;
static HWND hwndEdit ;
static PRBool testFinished = PR_FALSE;
static HWND     hwnd ;

LRESULT CALLBACK WinProc (HWND, UINT, WPARAM, LPARAM);

TCHAR appName[] = TEXT ("WinEvent") ;

int WINAPI WinMain(
    HINSTANCE hInstance, 
    HINSTANCE hPrevInstance,
    PSTR szCmdLine, 
    int iCmdShow
    )
{
    MSG      msg ;
    WNDCLASS wndclass ;
    HANDLE   hAccel ;
    
    PR_Init(0, 0, 0);

    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WinProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon( NULL, IDI_APPLICATION );
    wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
    wndclass.hbrBackground = (HBRUSH)GetStockObject( WHITE_BRUSH );
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = appName;
     
    if ( !RegisterClass( &wndclass ))
    {
        MessageBox( NULL, 
            TEXT( "This program needs Win32" ),
            appName, 
            MB_ICONERROR );
        return 0; 
    }
     
    hwnd = CreateWindow( appName, 
        appName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 
        CW_USEDEFAULT,
        CW_USEDEFAULT, 
        CW_USEDEFAULT,
        NULL, 
        NULL, 
        hInstance, 
        NULL);
     
     ShowWindow( hwnd, iCmdShow );
     UpdateWindow( hwnd ); 
     
     for(;;)
     {
        if ( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE  ))
        {
            if ( GetMessage( &msg, NULL, 0, 0 ))
            {
                if ( hDlgModeless == NULL || !IsDialogMessage( hDlgModeless, &msg ))
                {
                    if ( !TranslateAccelerator( hwnd, hAccel, &msg ))
                    {
                        TranslateMessage( &msg );
                        DispatchMessage( &msg );
                    } /* end if !TranslateAccelerator */
                } 
            }
            else
            {
                break;    
            } /* end if GetMessage() */
        } 
        else /* !PeekMessage */
        {
            PR_Sleep(50);
        }/* end if PeekMessage() */
     } /* end for() */

     PR_JoinThread( tThread );
     PL_DestroyEventQueue( padQueue );
     PR_Cleanup();
     return msg.wParam ;
}

LRESULT CALLBACK WinProc(
    HWND hwnd, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam
)
{
     switch (message)
     {
     case WM_CREATE :
          hwndEdit = CreateWindow(
                        TEXT( "edit" ), 
                        NULL,
                        WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | 
                            WS_BORDER | ES_LEFT | ES_MULTILINE |
                            ES_AUTOHSCROLL | ES_AUTOVSCROLL,
                         0, 0, 0, 0, 
                         hwnd, 
                         (HMENU)ID_EDIT,
                         ((LPCREATESTRUCT)lParam)->hInstance, 
                         NULL);
     
          /* Initialize Event Processing for NSPR
          ** Retrieve the event queue just created
          ** Create the TimerThread
          */

/*
          PL_InitializeEventsLib( "someName" );
          padQueue = PL_GetMainEventQueue();
*/
          padQueue = PL_CreateEventQueue("MainQueue", PR_GetCurrentThread());
		  PR_ASSERT( padQueue != NULL );
          tThread = PR_CreateThread( PR_USER_THREAD,
                    TimerThread,
                    NULL,
                    PR_PRIORITY_NORMAL,
                    PR_LOCAL_THREAD,
                    PR_JOINABLE_THREAD,
                    0 );
          return 0 ;
          
     case WM_SETFOCUS :
          SetFocus( hwndEdit );
          return 0;
          
     case WM_SIZE : 
          MoveWindow( hwndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE );
          return 0 ;
          
     case WM_COMMAND :
          if ( LOWORD(wParam) == ID_EDIT )
               if ( HIWORD(wParam ) == EN_ERRSPACE || 
                         HIWORD( wParam ) == EN_MAXTEXT )

                    MessageBox( hwnd, TEXT( "Edit control out of space." ),
                        appName, MB_OK | MB_ICONSTOP );
          return 0;
               
     case WM_DESTROY :
          PostQuitMessage(0);
          return 0;
     }
     return DefWindowProc( hwnd, message, wParam, lParam );
}



/*
** TimerThread() -- The Main function of the timer pop thread
**
*/
static void PR_CALLBACK TimerThread( void *arg )
{
    PRIntn  rc;

    do {
        PadEvent   *ev;
        
        /*
        ** Create and Post the event the event
        */
        PL_ENTER_EVENT_QUEUE_MONITOR( padQueue );
        ev = (PadEvent *) PR_NEW( PadEvent );
        PL_InitEvent( &ev->plEvent, NULL, 
                (PLHandleEventProc)HandlePadEvent, 
                (PLDestroyEventProc)DestroyPadEvent );
        PL_PostEvent( padQueue, &ev->plEvent );
        PL_EXIT_EVENT_QUEUE_MONITOR( padQueue );
            
        PR_Sleep( PR_MillisecondsToInterval(ThreadSleepTime) );
    } while( testFinished == PR_FALSE );

    PR_Sleep( PR_SecondsToInterval(4) );

    /*
    ** All done now. This thread can kill the main thread by sending 
    ** WM_DESTROY message to the main window.
    */
    SendMessage( hwnd, WM_DESTROY, 0, 0 );
    return;
}    

static char *startMessage = "Poppad: NSPR Windows GUI and event test program.\n"
                            "Every 1 second gets a '.'.\n"
                            "The test self terminates in less than a minute\n"
                            "You should be able to type in the window.\n\n";

static char *stopMessage = "\n\nIf you saw a series of dots being emitted in the window\n"
                           " at one second intervals, the test worked.\n\n";

/*
** HandlePadEvent() -- gets called because of PostEvent
*/
static void PR_CALLBACK HandlePadEvent( PadEvent *padEvent )
{
    char *cp;
    static const long lineLimit = 10;   /* limit on number of '.' per line */
    static const long timerLimit = 25;  /* limit on timer pop iterations */

    if ( timerCount++ == 0 )
    {
        
        for ( cp = startMessage; *cp != 0 ; cp++ )
        {
            SendMessage( hwndEdit, WM_CHAR, *cp, 1 );
        }
    }
    /* 
    ** Send a WM_CHAR event the edit Window
    */
    SendMessage( hwndEdit, WM_CHAR, '.', 1 );
    
    /*
    ** Limit the number of characters sent via timer pop to lineLimit
    */
    if ( (timerCount % lineLimit) == 0)
    {
        SendMessage( hwndEdit, WM_CHAR, '\n', 1 );
    }

    if ( timerCount >= timerLimit )
    {
        for ( cp = stopMessage; *cp != 0 ; cp++ )
        {
            SendMessage( hwndEdit, WM_CHAR, *cp, 1 );
        }
        testFinished = PR_TRUE;
    }

    return;
}

/*
** DestroyPadEvent() -- Called after HandlePadEvent()
*/
static void PR_CALLBACK DestroyPadEvent( PadEvent *padevent )
{
   PR_Free( padevent );
   return;
}    
