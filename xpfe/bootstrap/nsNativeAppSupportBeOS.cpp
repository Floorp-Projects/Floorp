/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Takashi Toyoshima <toyoshim@be-in.org>
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
#include "nsNativeAppSupportBase.h"
#include "nsString.h"
#include "nsICmdLineService.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIDOMWindowInternal.h"

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Bitmap.h>
#include <Screen.h>
#include <Resources.h>

#ifdef DEBUG
#define DEBUG_SPLASH 1
#endif

class nsSplashScreenBeOS : public nsISplashScreen {
public:
    nsSplashScreenBeOS();
    ~nsSplashScreenBeOS();

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

private:
    nsresult LoadBitmap();

    nsrefcnt mRefCnt;
    BWindow *window;
    BBitmap *bitmap;
}; // class nsSplashScreenBeOS

class nsNativeAppSupportBeOS : public nsNativeAppSupportBase {
public:
    // Overrides of base implementation.
    NS_IMETHOD Start( PRBool *aResult );
    NS_IMETHOD Stop( PRBool *aResult );
    NS_IMETHOD Quit();

private:
    nsrefcnt mRefCnt;
}; // nsNativeAppSupportBeOS

nsSplashScreenBeOS::nsSplashScreenBeOS()
    : mRefCnt( 0 ) , window( NULL ) , bitmap( NULL ) {
#ifdef DEBUG_SPLASH
    puts("nsSplashScreenBeOS::nsSlpashScreenBeOS()");
#endif
}

nsSplashScreenBeOS::~nsSplashScreenBeOS() {
#ifdef DEBUG_SPLASH
    puts("nsSplashScreenBeOS::~nsSlpashScreenBeOS()");
#endif
    Hide();
}

NS_IMETHODIMP
nsSplashScreenBeOS::Show() {
#ifdef DEBUG_SPLASH
    puts("nsSplashScreenBeOS::Show()");
#endif
    if (NULL == bitmap && NS_OK != LoadBitmap())
        return NS_ERROR_FAILURE;

    // Get the center position.
    BScreen scr;
    BRect scrRect = scr.Frame();
    BRect bmpRect = bitmap->Bounds();
    float winX = (scrRect.right - bmpRect.right) / 2;
    float winY = (scrRect.bottom - bmpRect.bottom) / 2;
    BRect winRect(winX, winY, winX + bmpRect.right, winY + bmpRect.bottom);
#ifdef DEBUG_SPLASH
    printf("SplashRect (%f, %f) - (%f, %f)\n", winRect.left, winRect.top,
	winRect.right, winRect.bottom);
#endif
    if (NULL == window) {
        window = new BWindow(winRect,
		"mozilla splash",
		B_NO_BORDER_WINDOW_LOOK,
		B_MODAL_APP_WINDOW_FEEL,
		0);
        if (NULL == window)
            return NS_ERROR_FAILURE;
        BView *view = new BView(bmpRect, "splash view", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
        if (NULL != view) {
            window->AddChild(view);
            view->SetViewBitmap(bitmap);
        }
        window->Show();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSplashScreenBeOS::Hide() {
#ifdef DEBUG_SPLASH
    puts("nsSplashScreenBeOS::Hide()");
#endif
    if (NULL != window) {
        if (window->Lock())
          window->Quit();
        window = NULL;
    }
    if (NULL != bitmap) {
        delete bitmap;
        bitmap = NULL;
    }
    return NS_OK;
}

nsresult
nsSplashScreenBeOS::LoadBitmap() {
    BResources *rsrc = be_app->AppResources();
    if (NULL == rsrc)
        return NS_ERROR_FAILURE;
    size_t length;
    const void *data = rsrc->LoadResource('BBMP', "MOZILLA:SPLASH", &length);
    if (NULL == data)
        return NS_ERROR_FAILURE;
    BMessage msg;
    if (B_OK != msg.Unflatten((const char *)data))
        return NS_ERROR_FAILURE;
    BBitmap *bmp = new BBitmap(&msg);
    if (NULL == bmp)
        return NS_ERROR_FAILURE;
    bitmap = new BBitmap(bmp, true);
    if (NULL == bitmap) {
        delete bmp;
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

// Create and return an instance of class nsNativeAppSupportBeOS.
nsresult
NS_CreateNativeAppSupport( nsINativeAppSupport **aResult ) {
    if ( aResult ) {
        *aResult = new nsNativeAppSupportBeOS();
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

// Create instance of BeOS splash screen object.
nsresult
NS_CreateSplashScreen( nsISplashScreen **aResult ) {
    if ( aResult ) {
        *aResult = new nsSplashScreenBeOS;
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

NS_IMETHODIMP
nsNativeAppSupportBeOS::Start( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );

    nsresult rv = NS_OK;
#ifdef DEBUG_SPLASH
    puts("nsNativeAppSupportBeOS::Start()");
#endif
    *aResult = PR_TRUE;

    return rv;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Stop( PRBool *aResult ) {
    NS_ENSURE_ARG( aResult );

    nsresult rv = NS_OK;
#ifdef DEBUG_SPLASH
    puts("nsNativeAppSupportBeOS::Stop()");
#endif
    *aResult = PR_TRUE;

    return rv;
}

NS_IMETHODIMP
nsNativeAppSupportBeOS::Quit() {
#ifdef DEBUG_SPLASH
    puts("nsNativeAppSupportBeOS::Quit()");
#endif

    return NS_OK;
}

PRBool NS_CanRun()
{
    return PR_TRUE;
}

