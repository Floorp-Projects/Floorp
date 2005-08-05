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
 * The Original Code is Minimo.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "MinimoPrivate.h"

#ifdef WINCE
#include "resource.h"

static HWND gSplashScreenDialog = NULL;

BOOL CALLBACK
SplashScreenDialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp ) 
{
  if ( msg == WM_INITDIALOG ) 
  {
    SetWindowText(dlg, "Minimo");
    
    gSplashScreenDialog = dlg;
    
    HWND bitmapControl = GetDlgItem( dlg, IDC_SPLASHBMP );
    if ( bitmapControl )
    {
      HBITMAP hbitmap = (HBITMAP)SendMessage( bitmapControl,
                                              STM_GETIMAGE,
                                              IMAGE_BITMAP,
                                              0 );
      if ( hbitmap ) 
      {
        BITMAP bitmap;
        if ( GetObject( hbitmap, sizeof bitmap, &bitmap ) )
        {
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
  } 
  else if (msg == WM_CLOSE)
  {
    NS_TIMELINE_MARK_FUNCTION("SplashScreenDialogProc Close");
    
    EndDialog(dlg, 0);
    gSplashScreenDialog = NULL;
  }
  return 0;
}

DWORD WINAPI SplashScreenThreadProc(LPVOID notused) 
{
  DialogBoxParamW( GetModuleHandle( 0 ),
                   MAKEINTRESOURCE( IDD_SPLASHSCREEN ),
                   HWND_DESKTOP,
                   (DLGPROC)SplashScreenDialogProc,
                   NULL );

  return 0;
}

void KillSplashScreen()
{
  NS_TIMELINE_MARK_FUNCTION("KillSplashScreen");

  if (gSplashScreenDialog) 
  {
    EndDialog((HWND)gSplashScreenDialog, 0);
    gSplashScreenDialog = NULL;
  }
}

void CreateSplashScreen()
{
  NS_TIMELINE_MARK_FUNCTION("CreateSplashScreen");

  DWORD threadID;
  HANDLE handle = CreateThread( 0, 
                                0, 
                                (LPTHREAD_START_ROUTINE)SplashScreenThreadProc, 
                                0, 
                                0, 
                                &threadID );
  CloseHandle(handle);
}


void GetScreenSize(unsigned long* x, unsigned long* y)
{
  RECT workarea;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &workarea, 0);

  *x = workarea.right - workarea.left;
  *y = workarea.bottom - workarea.top;
}

#else

void CreateSplashScreen() {}
void KillSplashScreen() {}
void GetScreenSize(unsigned long* x, unsigned long* y)
{
  *x = *y = 0;
}


#endif // WINCE
