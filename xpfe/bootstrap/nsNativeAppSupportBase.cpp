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
#include "nsNativeAppSupportBase.h"

nsNativeAppSupportBase::nsNativeAppSupportBase()
    : mRefCnt( 0 ),
      mSplash( 0 ),
      mServerMode( PR_FALSE ),
      mShouldShowUI( PR_TRUE ),
      mShownTurboDialog( PR_FALSE ) {
}

nsNativeAppSupportBase::~nsNativeAppSupportBase() {
}

// Start answer defaults to OK.
NS_IMETHODIMP
nsNativeAppSupportBase::Start( PRBool *result ) {
    NS_ENSURE_ARG( result );
    *result = PR_TRUE;
    return NS_OK;
}

// Stop answer defaults to OK.
NS_IMETHODIMP
nsNativeAppSupportBase::Stop( PRBool *result ) {
    NS_ENSURE_ARG( result );
    *result = PR_TRUE;
    return NS_OK;
}

// Quit is a 0x90.
NS_IMETHODIMP
nsNativeAppSupportBase::Quit() {
    return NS_OK;
}

// Show splash screen if implementation has one.
NS_IMETHODIMP
nsNativeAppSupportBase::ShowSplashScreen() {
    if ( !mSplash ) {
        nsresult rv = CreateSplashScreen( getter_AddRefs( mSplash ) );
    }
    if ( mSplash ) {
        mSplash->Show();
    }
    return NS_OK;
}

// Hide splash screen if there is one.
NS_IMETHODIMP
nsNativeAppSupportBase::HideSplashScreen() {
    // See if there is a splash screen object.
    if ( mSplash ) {
        // Unhook it.
        nsCOMPtr<nsISplashScreen> splash = mSplash;
        mSplash = 0;
        // Hide it.
        splash->Hide();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::SetIsServerMode(PRBool aIsServerMode) {
    mServerMode = aIsServerMode;
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::GetIsServerMode(PRBool *aIsServerMode) {
    NS_ENSURE_ARG( aIsServerMode );
    *aIsServerMode = mServerMode;
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::SetShouldShowUI(PRBool aShouldShowUI) {
    mShouldShowUI = aShouldShowUI;
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::GetShouldShowUI(PRBool *aShouldShowUI) {
    NS_ENSURE_ARG( aShouldShowUI );
    *aShouldShowUI = mShouldShowUI;
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::StartServerMode() {
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::OnLastWindowClosing(nsIXULWindow *aWindow) {
    return NS_OK;
}

// Default implementation doesn't have a splash screen.
NS_IMETHODIMP
nsNativeAppSupportBase::CreateSplashScreen( nsISplashScreen **splash ) {
    NS_ENSURE_ARG( splash );
    *splash = 0;
    return NS_CreateSplashScreen( splash );
}

// Standard implementations of AddRef/Release/QueryInterface.
NS_IMETHODIMP_(nsrefcnt)
nsNativeAppSupportBase::AddRef() {
    mRefCnt++;
    return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt)
nsNativeAppSupportBase::Release() {
    --mRefCnt;
    if ( !mRefCnt ) {
        delete this;
        return 0;
    }
    return mRefCnt;
}

NS_IMETHODIMP
nsNativeAppSupportBase::QueryInterface( const nsIID &iid, void**p ) {
    nsresult rv = NS_OK;
    if ( p ) {
        *p = 0;
        if ( iid.Equals( NS_GET_IID( nsINativeAppSupport ) ) ) {
            nsINativeAppSupport *result = this;
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

NS_IMETHODIMP
nsNativeAppSupportBase::StartAddonFeatures()
{
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::StopAddonFeatures()
{
    return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::EnsureProfile(nsICmdLineService* args)
{
    return NS_OK;
}
