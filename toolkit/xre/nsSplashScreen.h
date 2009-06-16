/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 *   mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef NS_SPLASHSCREEN_H_
#define NS_SPLASHSCREEN_H_

#include "prtypes.h"

/* Note: This is not XPCOM!  This class is used before any Gecko/XPCOM
 * support has been initialized, so any implementations should take care
 * to use platform-native methods as much as possible.
 */

class nsSplashScreen {
public:
    // An implementation needs to provide these, to either get
    // an existing splash screen, or create a new one if GetOrCreate is
    // used.
    static nsSplashScreen* GetOrCreate();
    static nsSplashScreen* Get();
public:
    // Display the splash screen if it's not already displayed.
    // Also resets progress to 0 and the message to empty.
    virtual void Open() = 0;
    virtual void Close() = 0;

    /* Update the splash screen to the given progress value (0..100) */
    virtual void Update(PRInt32 progress) = 0;

    PRBool IsOpen() { return mIsOpen; }

protected:
    nsSplashScreen() : mIsOpen(PR_FALSE) { }
    PRBool mIsOpen;
};

extern "C" {
    nsSplashScreen *NS_GetSplashScreen(PRBool create);
    typedef nsSplashScreen* (*NS_GetSplashScreenPtr) (PRBool);
}

#endif /* NS_SPLASHSCREEN_H_ */
