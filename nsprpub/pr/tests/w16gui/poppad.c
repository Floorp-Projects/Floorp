/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/*---------------------------------------
   POPPAD.C -- Popup Editor
   (c) Charles Petzold, 1992
  ---------------------------------------*/

#include "nspr.h"
#include "plevent.h"
#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include "poppad.h"
#include <time.h>

#define EDITID   1
#define UNTITLED "(untitled)"

long FAR PASCAL _export WndProc      (HWND, UINT, UINT, LONG) ;
BOOL FAR PASCAL _export AboutDlgProc (HWND, UINT, UINT, LONG) ;

/* Declarations for NSPR customization
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
static int  quitSwitch = 0;
static long ThreadSleepTime = 1000;
static long timerCount = 0;
static char *startMessage = "Poppad: NSPR GUI and event test program.\n"
                            "You should see lines of 50 characters\n"
                            "with a new character appearing at 1 second intervals.\n"
                            "Every 10 seconds gets a '+'; every 5 seconds gets a '_';\n"
                            "every 1 second gets a '.'.\n\n"
                            "You should be able to type in the window.\n\n\n";


          // Functions in POPFILE.C

void PopFileInitialize (HWND) ;
BOOL PopFileOpenDlg    (HWND, LPSTR, LPSTR) ;
BOOL PopFileSaveDlg    (HWND, LPSTR, LPSTR) ;
BOOL PopFileRead       (HWND, LPSTR) ;
BOOL PopFileWrite      (HWND, LPSTR) ;

          // Functions in POPFIND.C

HWND PopFindFindDlg     (HWND) ;
HWND PopFindReplaceDlg  (HWND) ;
BOOL PopFindFindText    (HWND, int *, LPFINDREPLACE) ;
BOOL PopFindReplaceText (HWND, int *, LPFINDREPLACE) ;
BOOL PopFindNextText    (HWND, int *) ;
BOOL PopFindValidFind   (void) ;

          // Functions in POPFONT.C

void PopFontInitialize   (HWND) ;
BOOL PopFontChooseFont   (HWND) ;
void PopFontSetFont      (HWND) ;
void PopFontDeinitialize (void) ;

          // Functions in POPPRNT.C

BOOL PopPrntPrintFile (HANDLE, HWND, HWND, LPSTR) ;

          // Global variables

static char szAppName [] = "PopPad" ;
static HWND hDlgModeless ;
static HWND hwndEdit ;

int PASCAL WinMain (HANDLE hInstance, HANDLE hPrevInstance,
                    LPSTR lpszCmdLine, int nCmdShow)
    {
    MSG      msg;
    HWND     hwnd ;
    HANDLE   hAccel ;
    WNDCLASS wndclass ;

    PR_STDIO_INIT();
    PR_Init(0, 0, 0);
          
    if (!hPrevInstance) 
          {
          wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
          wndclass.lpfnWndProc   = WndProc ;
          wndclass.cbClsExtra    = 0 ;
          wndclass.cbWndExtra    = 0 ;
          wndclass.hInstance     = hInstance ;
          wndclass.hIcon         = LoadIcon (hInstance, szAppName) ;
          wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
          wndclass.hbrBackground = GetStockObject (WHITE_BRUSH) ;
          wndclass.lpszMenuName  = szAppName ;
          wndclass.lpszClassName = szAppName ;

          RegisterClass (&wndclass) ;
          }

     hwnd = CreateWindow (szAppName, NULL,
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          NULL, NULL, hInstance, lpszCmdLine) ;

     ShowWindow (hwnd, nCmdShow) ;
     UpdateWindow (hwnd); 

     hAccel = LoadAccelerators (hInstance, szAppName) ;
     
     for(;;)
     {
        if ( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE  ))
        {
            if (GetMessage(&msg, NULL, 0, 0))
            {
                if (hDlgModeless == NULL || !IsDialogMessage (hDlgModeless, &msg))
                {
                    if (!TranslateAccelerator (hwnd, hAccel, &msg))
                    {
                        TranslateMessage (&msg) ;
                        DispatchMessage (&msg) ;
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

void DoCaption (HWND hwnd, char *szTitleName)
     {
     char szCaption [64 + _MAX_FNAME + _MAX_EXT] ;

     wsprintf (szCaption, "%s - %s", (LPSTR) szAppName,
               (LPSTR) (szTitleName [0] ? szTitleName : UNTITLED)) ;

     SetWindowText (hwnd, szCaption) ;
     }

void OkMessage (HWND hwnd, char *szMessage, char *szTitleName)
     {
     char szBuffer [64 + _MAX_FNAME + _MAX_EXT] ;

     wsprintf (szBuffer, szMessage,
               (LPSTR) (szTitleName [0] ? szTitleName : UNTITLED)) ;

     MessageBox (hwnd, szBuffer, szAppName, MB_OK | MB_ICONEXCLAMATION) ;
     }

short AskAboutSave (HWND hwnd, char *szTitleName)
     {
     char  szBuffer [64 + _MAX_FNAME + _MAX_EXT] ;
     short nReturn ;

     wsprintf (szBuffer, "Save current changes in %s?",
               (LPSTR) (szTitleName [0] ? szTitleName : UNTITLED)) ;

     nReturn = MessageBox (hwnd, szBuffer, szAppName,
                           MB_YESNOCANCEL | MB_ICONQUESTION) ;

     if (nReturn == IDYES)
          if (!SendMessage (hwnd, WM_COMMAND, IDM_SAVE, 0L))
               nReturn = IDCANCEL ;

     return nReturn ;
     }

long FAR PASCAL _export WndProc (HWND hwnd, UINT message, UINT wParam,
                                                          LONG lParam)
     {
     static BOOL    bNeedSave = FALSE ;
     static char    szFileName  [_MAX_PATH] ;
     static char    szTitleName [_MAX_FNAME + _MAX_EXT] ;
     static FARPROC lpfnAboutDlgProc ;
     static HANDLE  hInst ;
     static int     iOffset ;
     static UINT    messageFindReplace ;
     LONG           lSelect ;
     LPFINDREPLACE  lpfr ;
     WORD           wEnable ;

     switch (message)
          {
          case WM_CREATE:
                         // Get About dialog instance address

               hInst = ((LPCREATESTRUCT) lParam)->hInstance ;
               lpfnAboutDlgProc = MakeProcInstance ((FARPROC) AboutDlgProc,
                                                    hInst) ;

                         // Create the edit control child window

               hwndEdit = CreateWindow ("edit", NULL,
                         WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL |
                              WS_BORDER | ES_LEFT | ES_MULTILINE |
                              ES_NOHIDESEL | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
                         0, 0, 0, 0,
                         hwnd, EDITID, hInst, NULL) ;

               SendMessage (hwndEdit, EM_LIMITTEXT, 32000, 0L) ;

                         // Initialize common dialog box stuff

               PopFileInitialize (hwnd) ;
               PopFontInitialize (hwndEdit) ;

               messageFindReplace = RegisterWindowMessage (FINDMSGSTRING) ;

                         // Process command line

               lstrcpy (szFileName, (LPSTR)
                        (((LPCREATESTRUCT) lParam)->lpCreateParams)) ;

               if (lstrlen (szFileName) > 0)
                    {
                    GetFileTitle (szFileName, szTitleName,
                                  sizeof (szTitleName)) ;

                    if (!PopFileRead (hwndEdit, szFileName))
                         OkMessage (hwnd, "File %s cannot be read!",
                                          szTitleName) ;
                    }

               DoCaption (hwnd, szTitleName) ;

               /* Initialize Event Processing for NSPR
               ** Retrieve the event queue just created
               ** Create the TimerThread
               */               
               PL_InitializeEventsLib("someName");
               padQueue = PL_GetMainEventQueue();
               tThread = PR_CreateThread(PR_USER_THREAD,
                        TimerThread,
                        NULL,
                        PR_PRIORITY_NORMAL,
                        PR_LOCAL_THREAD,
                        PR_JOINABLE_THREAD,
                        0 );
               return 0 ;

          case WM_SETFOCUS:
               SetFocus (hwndEdit) ;
               return 0 ;

          case WM_SIZE: 
               MoveWindow (hwndEdit, 0, 0, LOWORD (lParam),
                                           HIWORD (lParam), TRUE) ;
               return 0 ;

          case WM_INITMENUPOPUP:
               switch (lParam)
                    {
                    case 1:        // Edit menu

                              // Enable Undo if edit control can do it

                         EnableMenuItem (wParam, IDM_UNDO,
                              SendMessage (hwndEdit, EM_CANUNDO, 0, 0L) ?
                                   MF_ENABLED : MF_GRAYED) ;

                              // Enable Paste if text is in the clipboard

                         EnableMenuItem (wParam, IDM_PASTE,
                              IsClipboardFormatAvailable (CF_TEXT) ?
                                   MF_ENABLED : MF_GRAYED) ;

                              // Enable Cut, Copy, and Del if text is selected

                         lSelect = SendMessage (hwndEdit, EM_GETSEL, 0, 0L) ;
                         wEnable = HIWORD (lSelect) != LOWORD (lSelect) ?
                                        MF_ENABLED : MF_GRAYED ;

                         EnableMenuItem (wParam, IDM_CUT,  wEnable) ;
                         EnableMenuItem (wParam, IDM_COPY, wEnable) ;
                         EnableMenuItem (wParam, IDM_DEL,  wEnable) ;
                         break ;

                    case 2:        // Search menu

                              // Enable Find, Next, and Replace if modeless
                              //   dialogs are not already active

                         wEnable = hDlgModeless == NULL ?
                                        MF_ENABLED : MF_GRAYED ;

                         EnableMenuItem (wParam, IDM_FIND,    wEnable) ;
                         EnableMenuItem (wParam, IDM_NEXT,    wEnable) ;
                         EnableMenuItem (wParam, IDM_REPLACE, wEnable) ;
                         break ;
                    }
               return 0 ;

          case WM_COMMAND :
                              // Messages from edit control

               if (LOWORD (lParam) && wParam == EDITID)
                    {
                    switch (HIWORD (lParam))
                         {
                         case EN_UPDATE:
                              bNeedSave = TRUE ;
                              return 0 ;

                         case EN_ERRSPACE:
                         case EN_MAXTEXT:
                              MessageBox (hwnd, "Edit control out of space.",
                                        szAppName, MB_OK | MB_ICONSTOP) ;
                              return 0 ;
                         }
                    break ;
                    }

               switch (wParam)
                    {
                              // Messages from File menu

                    case IDM_NEW:
                         if (bNeedSave && IDCANCEL ==
                                   AskAboutSave (hwnd, szTitleName))
                              return 0 ;

                         SetWindowText (hwndEdit, "\0") ;
                         szFileName [0]  = '\0' ;
                         szTitleName [0] = '\0' ;
                         DoCaption (hwnd, szTitleName) ;
                         bNeedSave = FALSE ;
                         return 0 ;

                    case IDM_OPEN:
                         if (bNeedSave && IDCANCEL ==
                                   AskAboutSave (hwnd, szTitleName))
                              return 0 ;

                         if (PopFileOpenDlg (hwnd, szFileName, szTitleName))
                              {
                              if (!PopFileRead (hwndEdit, szFileName))
                                   {
                                   OkMessage (hwnd, "Could not read file %s!",
                                                    szTitleName) ;
                                   szFileName  [0] = '\0' ;
                                   szTitleName [0] = '\0' ;
                                   }
                              }

                         DoCaption (hwnd, szTitleName) ;
                         bNeedSave = FALSE ;
                         return 0 ;

                    case IDM_SAVE:
                         if (szFileName [0])
                              {
                              if (PopFileWrite (hwndEdit, szFileName))
                                   {
                                   bNeedSave = FALSE ;
                                   return 1 ;
                                   }
                              else
                                   OkMessage (hwnd, "Could not write file %s",
                                                    szTitleName) ;
                              return 0 ;
                              }
                                                  // fall through
                    case IDM_SAVEAS:
                         if (PopFileSaveDlg (hwnd, szFileName, szTitleName))
                              {
                              DoCaption (hwnd, szTitleName) ;

                              if (PopFileWrite (hwndEdit, szFileName))
                                   {
                                   bNeedSave = FALSE ;
                                   return 1 ;
                                   }
                              else
                                   OkMessage (hwnd, "Could not write file %s",
                                                    szTitleName) ;
                              }
                         return 0 ;

                    case IDM_PRINT:
                         if (!PopPrntPrintFile (hInst, hwnd, hwndEdit,
                                                szTitleName))
                              OkMessage (hwnd, "Could not print file %s",
                                         szTitleName) ;
                         return 0 ;

                    case IDM_EXIT:
                         SendMessage (hwnd, WM_CLOSE, 0, 0L) ;
                         return 0 ;

                              // Messages from Edit menu

                    case IDM_UNDO:
                         SendMessage (hwndEdit, WM_UNDO, 0, 0L) ;
                         return 0 ;

                    case IDM_CUT:
                         SendMessage (hwndEdit, WM_CUT, 0, 0L) ;
                         return 0 ;

                    case IDM_COPY:
                         SendMessage (hwndEdit, WM_COPY, 0, 0L) ;
                         return 0 ;

                    case IDM_PASTE:
                         SendMessage (hwndEdit, WM_PASTE, 0, 0L) ;
                         return 0 ;

                    case IDM_DEL:
                         SendMessage (hwndEdit, WM_CLEAR, 0, 0L) ;
                         return 0 ;

                    case IDM_SELALL:
                         SendMessage (hwndEdit, EM_SETSEL, 0,
                                        MAKELONG (0, 32767)) ;
                         return 0 ;

                              // Messages from Search menu

                    case IDM_FIND:
                         iOffset = HIWORD (
                              SendMessage (hwndEdit, EM_GETSEL, 0, 0L)) ;
                         hDlgModeless = PopFindFindDlg (hwnd) ;
                         return 0 ;

                    case IDM_NEXT:
                         iOffset = HIWORD (
                              SendMessage (hwndEdit, EM_GETSEL, 0, 0L)) ;

                         if (PopFindValidFind ())
                              PopFindNextText (hwndEdit, &iOffset) ;
                         else
                              hDlgModeless = PopFindFindDlg (hwnd) ;

                         return 0 ;

                    case IDM_REPLACE:
                         iOffset = HIWORD (
                              SendMessage (hwndEdit, EM_GETSEL, 0, 0L)) ;

                         hDlgModeless = PopFindReplaceDlg (hwnd) ;
                         return 0 ;

                    case IDM_FONT:
                         if (PopFontChooseFont (hwnd))
                              PopFontSetFont (hwndEdit) ;

                         return 0 ;

                              // Messages from Help menu

                    case IDM_HELP:
                         OkMessage (hwnd, "Help not yet implemented!", NULL) ;
                         return 0 ;

                    case IDM_ABOUT:
                         DialogBox (hInst, "AboutBox", hwnd, lpfnAboutDlgProc);
                         return 0 ;
                    }
               break ;

          case WM_CLOSE:
               if (!bNeedSave || IDCANCEL != AskAboutSave (hwnd, szTitleName))
                    DestroyWindow (hwnd) ;

               return 0 ;

          case WM_QUERYENDSESSION:
               if (!bNeedSave || IDCANCEL != AskAboutSave (hwnd, szTitleName))
                    return 1L ;

               return 0 ;

          case WM_DESTROY:
               PopFontDeinitialize () ;
               PostQuitMessage (0) ;
               quitSwitch = 1;
               return 0 ;

          default:
                         // Process "Find-Replace" messages

               if (message == messageFindReplace)
                    {
                    lpfr = (LPFINDREPLACE) lParam ;

                    if (lpfr->Flags & FR_DIALOGTERM)
                         hDlgModeless = NULL ;

                    if (lpfr->Flags & FR_FINDNEXT)
                         if (!PopFindFindText (hwndEdit, &iOffset, lpfr))
                              OkMessage (hwnd, "Text not found!", NULL) ;

                    if (lpfr->Flags & FR_REPLACE ||
                        lpfr->Flags & FR_REPLACEALL)
                         if (!PopFindReplaceText (hwndEdit, &iOffset, lpfr))
                              OkMessage (hwnd, "Text not found!", NULL) ;

                    if (lpfr->Flags & FR_REPLACEALL)
                         while (PopFindReplaceText (hwndEdit, &iOffset, lpfr));

                    return 0 ;
                    }
               break ;
          }
     return DefWindowProc (hwnd, message, wParam, lParam) ;
     }

BOOL FAR PASCAL _export AboutDlgProc (HWND hDlg, UINT message, UINT wParam,
                                                               LONG lParam)
     {
     switch (message)
          {
          case WM_INITDIALOG:
               return TRUE ;

          case WM_COMMAND:
               switch (wParam)
                    {
                    case IDOK:
                         EndDialog (hDlg, 0) ;
                         return TRUE ;
                    }
               break ;
          }
     return FALSE ;
     }
/*
** TimerThread() -- The Main function of the timer pop thread
**
*/
static void PR_CALLBACK TimerThread( void *arg)
{
    do {
        PadEvent   *ev;
        
        /*
        ** Should we quit now?
        */
        if ( quitSwitch )
            break;
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
            
        PR_Sleep( ThreadSleepTime );
    } while(1);
    return;
}    

/*
** HandlePadEvent() -- gets called because of PostEvent
*/
static void PR_CALLBACK HandlePadEvent( PadEvent *padEvent )
{
    if ( timerCount++ == 0 )
    {
        char *cp;
        
        for ( cp = startMessage; *cp != 0 ; cp++ )
        {
            SendMessage( hwndEdit, WM_CHAR, *cp, MAKELONG( *cp, 1 ));
        }
    }
    /* 
    ** Send a WM_CHAR event the edit Window
    */
    if ((timerCount % 10) == 0)
    {
        SendMessage( hwndEdit, WM_CHAR, '+', MAKELONG( '+', 1 ));
    }
    else if ((timerCount % 5) == 0)
    {
        SendMessage( hwndEdit, WM_CHAR, '_', MAKELONG( '_', 1 ));
    }
    else
    {
        SendMessage( hwndEdit, WM_CHAR, '.', MAKELONG( '.', 1 ));
    }
    
    if ( (timerCount % 50) == 0)
    {
        SendMessage( hwndEdit, WM_CHAR, '\n', MAKELONG( '\n', 1 ));
    }

    /*
    ** PL_RevokeEvents() is broken. Test to fix it.
    */
    {
        static long revokeCounter = 0;

        if (revokeCounter++ > 10 )
        {
            PR_Sleep( ThreadSleepTime * 10 );
            SendMessage( hwndEdit, WM_CHAR, '*', MAKELONG( '\n', 1 ));
            PL_RevokeEvents( padQueue, NULL );
            revokeCounter = 0;
        }
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
