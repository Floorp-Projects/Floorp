/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 */
#include "nsNativeAppSupport.h"
#include "nsNativeAppSupportWin.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

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
    static void CheckConsole();
    static nsSplashScreenWin* GetPointer( HWND dlg );

    static BOOL CALLBACK DialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp );
    static BOOL CALLBACK PhantomDialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp );
    static DWORD WINAPI ThreadProc( LPVOID );

    HWND mDlg;
    HBITMAP mBitmap;
    nsrefcnt mRefCnt;
}; // class nsSplashScreenWin

nsSplashScreenWin::nsSplashScreenWin()
    : mDlg( 0 ), mBitmap( 0 ), mRefCnt( 0 ) {
}

nsSplashScreenWin::~nsSplashScreenWin() {
#ifdef DEBUG_law
    printf( "splash screen dtor called\n" );
#endif
    // Make sure dialog is gone.
    Hide();
}

NS_IMETHODIMP
nsSplashScreenWin::Show() {
    /*
     * A hack for http://bugzilla.mozilla.org/show_bug.cgi?id=26581
     *
     * Windows NT seems to think that the first thread to show a window must 
     * therefore be our main UI thread. It then seems to want to put the first 
     * window that shows up on other threads in our application behind the main 
     * windows of other applications. Since we want to show our splash screen 
     * on a 'non-main' thread, our browser windows start showing up later on a 
     * thread that Windows thinks is not our main UI thread. So, (I believe) it 
     * pushes the first one of our browser windows to the back until the user 
     * pulls it forward. That is not what we want!
     * 
     * This hack attempts to trick Windows by very briefly showing an 
     * offscreen window on the main thread. This happens before the splash 
     * screen thread creates its window (which it sets as topmost). So when our
     * browser windows start showing up on the main thread Windows will see that 
     * they are on the "first thread with a window" and it will (hopefully!) not
     * push them to the back.
     */
    DialogBox( GetModuleHandle( 0 ),
               MAKEINTRESOURCE( IDD_SPLASH ),
               HWND_DESKTOP,
               (DLGPROC)PhantomDialogProc );

    // Spawn new thread to display real splash screen.
    DWORD threadID = 0;
    CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)ThreadProc, this, 0, &threadID );
    return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenWin::Hide() {
    if ( mDlg ) {
        // Dismiss the dialog.
        EndDialog( mDlg, 0 );
        // Release custom bitmap (if there is one).
        if ( mBitmap ) {
            BOOL ok = DeleteObject( mBitmap );
        }
        mBitmap = 0;
        mDlg = 0;
    }
    return NS_OK;
}

BOOL CALLBACK
nsSplashScreenWin::PhantomDialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp ) {
    if ( msg == WM_INITDIALOG ) {
        // Show window for an instant to make this the active thread.
        ShowWindow( dlg, SW_SHOW );
        EndDialog( dlg, 0 );
        return 1;
    } 
    return 0;
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
nsSplashScreenWin::CheckConsole() {
    for ( int i = 1; i < __argc; i++ ) {
        if ( strcmp( "-console", __argv[i] ) == 0
             ||
             strcmp( "/console", __argv[i] ) == 0 ) {
            // Users wants to make sure we have a console.
            // Try to allocate one.
            BOOL rc = AllocConsole();
            if ( rc ) {
                // Console allocated.  Fix it up so that output works in
                // all cases.  See http://support.microsoft.com/support/kb/articles/q105/3/05.asp.

                // stdout
                int hCrt = _open_osfhandle( (long)GetStdHandle( STD_OUTPUT_HANDLE ),
                                            _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = _fdopen( hCrt, "w" );
                    if ( hf ) {
                        *stdout = *hf;
                        fprintf( stdout, "stdout directed to dynamic console\n" );
                    }
                }

                // stderr
                hCrt = _open_osfhandle( (long)GetStdHandle( STD_ERROR_HANDLE ),
                                        _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = _fdopen( hCrt, "w" );
                    if ( hf ) {
                        *stderr = *hf;
                        fprintf( stderr, "stderr directed to dynamic console\n" );
                    }
                }

                // stdin?
                /* Don't bother for now.
                hCrt = _open_osfhandle( (long)GetStdHandle( STD_INPUT_HANDLE ),
                                        _O_TEXT );
                if ( hCrt != -1 ) {
                    FILE *hf = _fdopen( hCrt, "r" );
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

nsresult NS_CreateSplashScreen( nsISplashScreen **aResult ) {
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

    // Check for dynamic console creation request.
    nsSplashScreenWin::CheckConsole();

    return NS_OK;
}

PRBool NS_CanRun() 
{
	return PR_TRUE;
}
