/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIBrowserWindow_h___
#define nsIBrowserWindow_h___

#include "nsweb.h"
#include "nsISupports.h"

class nsIAppShell;
class nsIPref;
class nsIFactory;
class nsIWebShell;
class nsString;
struct nsRect;

#define NS_IBROWSER_WINDOW_IID \
 { 0xa6cf905c, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

#define NS_BROWSER_WINDOW_CID \
 { 0xa6cf905d, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Chrome mask
#define NS_CHROME_DEFAULT_CHROME      0x00000001
#define NS_CHROME_WINDOW_BORDERS_ON   0x00000002
#define NS_CHROME_WINDOW_CLOSE_ON     0x00000004
#define NS_CHROME_WINDOW_RESIZE_ON    0x00000008
#define NS_CHROME_MENU_BAR_ON         0x00000010
#define NS_CHROME_TOOL_BAR_ON         0x00000020
#define NS_CHROME_LOCATION_BAR_ON     0x00000040
#define NS_CHROME_STATUS_BAR_ON       0x00000080
#define NS_CHROME_PERSONAL_TOOLBAR_ON 0x00000100
#define NS_CHROME_SCROLLBARS_ON       0x00000200
#define NS_CHROME_TITLEBAR_ON         0x00000400
#define NS_CHROME_MODAL               0x20000000
#define NS_CHROME_OPEN_AS_DIALOG      0x40000000
#define NS_CHROME_OPEN_AS_CHROME      0x80000000
#define NS_CHROME_ALL_CHROME          0x000007FE

/**
 * API to a "browser window". A browser window contains a toolbar, a web shell
 * and a status bar. The toolbar and status bar are optional and are
 * controlled by the chrome API.
 */
class nsIBrowserWindow : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IBROWSER_WINDOW_IID; return iid; }
  NS_IMETHOD Init(nsIAppShell* aAppShell,
                  nsIPref* aPrefs,
                  const nsRect& aBounds,
                  PRUint32 aChromeMask,
                  PRBool aAllowPlugins = PR_TRUE) = 0;

  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY) = 0;

  NS_IMETHOD SizeWindowTo(PRInt32 aWidth, PRInt32 aHeight) = 0;
  NS_IMETHOD SizeContentTo(PRInt32 aWidth, PRInt32 aHeight) = 0;

  NS_IMETHOD GetContentBounds(nsRect& aResult) = 0;
  NS_IMETHOD GetWindowBounds(nsRect& aResult) = 0;

  NS_IMETHOD IsIntrinsicallySized(PRBool& aResult) = 0;

  NS_IMETHOD ShowAfterCreation() = 0;

  NS_IMETHOD Show() = 0;

  NS_IMETHOD Hide() = 0;

  NS_IMETHOD Close() = 0;

  NS_IMETHOD SetChrome(PRUint32 aNewChromeMask) = 0;

  NS_IMETHOD GetChrome(PRUint32& aChromeMaskResult) = 0;

  NS_IMETHOD SetTitle(const PRUnichar* aTitle) = 0;

  NS_IMETHOD GetTitle(const PRUnichar** aResult) = 0;

  NS_IMETHOD SetStatus(const PRUnichar* aStatus) = 0;

  NS_IMETHOD GetStatus(const PRUnichar** aResult) = 0;

  NS_IMETHOD SetDefaultStatus(const PRUnichar* aStatus) = 0;

  NS_IMETHOD GetDefaultStatus(const PRUnichar** aResult) = 0;

  NS_IMETHOD SetProgress(PRInt32 aProgress, PRInt32 aProgressMax) = 0;

  NS_IMETHOD ShowMenuBar(PRBool aShow) = 0;

  NS_IMETHOD GetWebShell(nsIWebShell*& aResult) = 0;
  NS_IMETHOD GetContentWebShell(nsIWebShell **aResult) = 0;

  // XXX minimize, maximize
  // XXX event control: enable/disable window close box, stick to glass, modal
};

#endif /* nsIBrowserWindow_h___ */
