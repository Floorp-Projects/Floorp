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
    static nsSplashScreenWin* GetPointer( HWND dlg );

    static BOOL CALLBACK DialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp );
    static DWORD WINAPI ThreadProc( LPVOID );

    HWND mDlg;
    nsrefcnt mRefCnt;
}; // class nsSplashScreenWin

nsSplashScreenWin::nsSplashScreenWin()
    : mRefCnt( 0 ), mDlg( 0 ) {
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
    // Spawn new thread to display splash screen.
    DWORD threadID = 0;
#ifdef DEBUG_law
    ThreadProc( this );
#else
    CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)ThreadProc, this, 0, &threadID );
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenWin::Hide() {
    if ( mDlg ) {
        // Dismiss the dialog.
        EndDialog( mDlg, 0 );
        mDlg = 0;
    }
    return NS_OK;
}

BOOL CALLBACK nsSplashScreenWin::DialogProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp ) {
    if ( msg == WM_INITDIALOG ) {
        // Store dialog handle.
        nsSplashScreenWin *splashScreen = (nsSplashScreenWin*)lp;
        if ( lp ) {
            splashScreen->SetDialog( dlg );
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
#ifdef DEBUG_law
    HWND dlg = CreateDialogParam( GetModuleHandle( 0 ),
                    MAKEINTRESOURCE( IDD_SPLASH ),
                    HWND_DESKTOP,
                    (DLGPROC)DialogProc,
                    (LPARAM)splashScreen );
    ShowWindow( dlg, SW_SHOW );
#else
    DialogBoxParam( GetModuleHandle( 0 ),
                    MAKEINTRESOURCE( IDD_SPLASH ),
                    HWND_DESKTOP,
                    (DLGPROC)DialogProc,
                    (LPARAM)splashScreen );
#endif
    return 0;
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
    return NS_OK;
}

PRBool NS_CanRun() 
{
	return PR_TRUE;
}