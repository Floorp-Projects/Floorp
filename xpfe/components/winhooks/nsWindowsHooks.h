/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nswindowshooks_h____
#define nswindowshooks_h____

#include "nsIWindowsHooks.h"

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
}; // nsWindowsHooksSettings

#endif // nswindowshooks_h____
