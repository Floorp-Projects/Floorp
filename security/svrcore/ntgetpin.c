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
 * The Original Code is the Netscape svrcore library.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1996
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

/******************************************************
 *
 *  ntgetpin.c - Prompts for the key
 *  database passphrase.
 *
 ******************************************************/

#if defined( WIN32 ) 

#include <windows.h>
#include <nspr.h>
#include "ntresource.h"

#undef Debug
#undef OFF
#undef LITTLE_ENDIAN

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

static char password[512];

static void CenterDialog(HWND hwndParent, HWND hwndDialog)
{
    RECT DialogRect;
    RECT ParentRect;
    POINT Point;
    int nWidth;
    int nHeight;
    
    // Determine if the main window exists. This can be useful when
    // the application creates the dialog box before it creates the
    // main window. If it does exist, retrieve its size to center
    // the dialog box with respect to the main window.
    if( hwndParent != NULL ) 
	{
		GetClientRect(hwndParent, &ParentRect);
    } 
	else 
	{
		// if main window does not exist, center with respect to desktop
		hwndParent = GetDesktopWindow();
		GetWindowRect(hwndParent, &ParentRect);
    }
    
    // get the size of the dialog box
    GetWindowRect(hwndDialog, &DialogRect);
    
    // calculate height and width for MoveWindow()
    nWidth = DialogRect.right - DialogRect.left;
    nHeight = DialogRect.bottom - DialogRect.top;
    
    // find center point and convert to screen coordinates
    Point.x = (ParentRect.right - ParentRect.left) / 2;
    Point.y = (ParentRect.bottom - ParentRect.top) / 2;
    
    ClientToScreen(hwndParent, &Point);
    
    // calculate new X, Y starting point
    Point.x -= nWidth / 2;
    Point.y -= nHeight / 2;
    
    MoveWindow(hwndDialog, Point.x, Point.y, nWidth, nHeight, FALSE);
}

static BOOL CALLBACK PinDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message) 
	{
      case WM_INITDIALOG:
				SetDlgItemText( hDlg, IDC_TOKEN_NAME, (char *)lParam);
				CenterDialog(NULL, hDlg);
				SendDlgItemMessage(hDlg, IDEDIT, EM_SETLIMITTEXT, sizeof(password), 0);
				EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
				return(FALSE);
	
      case WM_COMMAND:
				if(LOWORD(wParam) == IDEDIT) 
				{
					if(HIWORD(wParam) == EN_CHANGE) 
					{
						if(GetDlgItemText(hDlg, IDEDIT, password,
								  sizeof(password)) > 0) 
						{
							EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
						} 
						else 
						{
							EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
						}
					}
					return (FALSE);
				} 
				else if(LOWORD(wParam) == IDOK) 
				{
					GetDlgItemText(hDlg, IDEDIT, password, sizeof(password));
					EndDialog(hDlg, IDOK);
					return (TRUE);
				} 
				else if(LOWORD(wParam) == IDCANCEL) 
				{
					memset(password, 0, sizeof(password));
					EndDialog(hDlg, IDCANCEL);
					return(FALSE);
				}
    }
    return (FALSE);
}
char*
NT_PromptForPin (const char *tokenName)
{
    int iResult = 0;
    
    iResult = DialogBoxParam( GetModuleHandle( NULL ), 
	MAKEINTRESOURCE(IDD_DATABASE_PASSWORD),
	HWND_DESKTOP, (DLGPROC) PinDialogProc, (LPARAM)tokenName);
    if( iResult == -1 ) 
	{
		iResult = GetLastError();
/*
		ReportSlapdEvent( EVENTLOG_INFORMATION_TYPE, 
			MSG_SERVER_PASSWORD_DIALOG_FAILED, 0, NULL );
*/
		return NULL;
    }
    /* Return no-response if the user click on cancel */
    if (password[0] == 0) return 0;
    return strdup(password);
}

#endif /* defined( WIN32 )  */
