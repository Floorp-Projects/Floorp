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
 *   Bill Law       law@netscape.com
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
#include "nsNativeAppSupport.h"

#include "nsCOMPtr.h"

// nsNativeAppSupportBase
//
// This is a default implementation of the nsINativeAppSupport interface
// declared in mozilla/xpfe/appshell/public/nsINativeAppSupport.h.
//
// This implementation manages the reference counting and provides the
// default implementation of the various "native app support" functions
// (which will likely work for all platforms save Win32).
//
// Derived classes must implement the method CreateSplashScreen() in
// order to provide a splash screen for their platform.
class nsNativeAppSupportBase : public nsINativeAppSupport {
public:
    nsNativeAppSupportBase();
    ~nsNativeAppSupportBase();

    // nsINativeAppSupport methods.
    NS_DECL_NSINATIVEAPPSUPPORT

    NS_IMETHOD CreateSplashScreen( nsISplashScreen **splash );

    // nsISupports methods
    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();
    NS_IMETHOD QueryInterface( const nsIID &iid, void**p );

    nsrefcnt mRefCnt;
    nsCOMPtr<nsISplashScreen> mSplash;
    PRBool   mServerMode;
    PRBool   mShouldShowUI;
    PRBool   mShownTurboDialog;
}; // class nsSplashScreenWin

