/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nswindowshooks_h____
#define nswindowshooks_h____

#include "nsIWindowsHooks.h"

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

/* c09bc130-0a71-11d4-8076-00600811a9c3 */
#define NS_IWINDOWSHOOKS_CID \
 { 0xc09bc130, 0x0a71, 0x11d4, {0x80, 0x76, 0x00, 0x60, 0x08, 0x11, 0xa9, 0xc3} }

class nsWindowsHooksSettings : public nsIWindowsHooksSettings {
public:
    // ctor/dtor
    nsWindowsHooksSettings();
    virtual ~nsWindowsHooksSettings();

    // Declare all interface methods we must implement.
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWINDOWSHOOKSSETTINGS

    // Typedef for nsIWindowsHooksSettings getter/setter member functions.
    typedef nsresult (__stdcall nsIWindowsHooksSettings::*getter)( PRBool* );
    typedef nsresult (__stdcall nsIWindowsHooksSettings::*setter)( PRBool );

protected:
    // General purpose getter.
    NS_IMETHOD Get( PRBool *result, PRBool nsWindowsHooksSettings::*member );
    // General purpose setter.
    NS_IMETHOD Set( PRBool value, PRBool nsWindowsHooksSettings::*member );

private:
    // Internet shortcut protocols.
    struct {
        PRBool mHandleHTTP;
        PRBool mHandleHTTPS;
        PRBool mHandleFTP;
        PRBool mHandleCHROME;
        PRBool mHandleGOPHER;
    };
    // File types.
    struct {
        PRBool mHandleHTML;
        PRBool mHandleJPEG;
        PRBool mHandleGIF;
        PRBool mHandlePNG;
        PRBool mHandleXML;
        PRBool mHandleXUL;
    };
    struct {
        PRBool mShowDialog;
    };
    // Special member to handle initialization.
    PRBool mHaveBeenSet;
    NS_IMETHOD GetHaveBeenSet( PRBool * );
    NS_IMETHOD SetHaveBeenSet( PRBool );

    // Give nsWindowsHooks full access.
    friend class nsWindowsHooks;
}; // nsWindowsHooksSettings

class nsWindowsHooks : public nsIWindowsHooks {
public:
    // ctor/dtor
    nsWindowsHooks();
    virtual ~nsWindowsHooks();

    // Declare all interface methods we must implement.
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWINDOWSHOOKS

protected:
    // Internal flavor of GetPreferences.
    NS_IMETHOD GetSettings( nsWindowsHooksSettings ** );

    // Set registry according to settings.
    NS_IMETHOD SetRegistry();
    char mShortcutPath[MAX_BUF];
    char mShortcutName[MAX_BUF];
    char mShortcutBase[MAX_BUF];
    char mShortcutProg[MAX_BUF];
}; // nsWindowsHooksSettings

#endif // nswindowshooks_h____
