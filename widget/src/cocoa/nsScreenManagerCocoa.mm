/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsScreenManagerCocoa.h"
#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
#include "nsCocoaUtils.h"

NS_IMPL_ISUPPORTS1(nsScreenManagerCocoa, nsIScreenManager)

nsScreenManagerCocoa::nsScreenManagerCocoa()
{
}

nsScreenManagerCocoa::~nsScreenManagerCocoa()
{
}

nsScreenCocoa*
nsScreenManagerCocoa::ScreenForCocoaScreen (NSScreen *screen)
{
    for (PRInt32 i = 0; i < mScreenList.Count(); i++) {
        nsScreenCocoa* sc = static_cast<nsScreenCocoa*>(mScreenList.ObjectAt(i));

        if (sc->CocoaScreen() == screen) {
            // doesn't addref
            return sc;
        }
    }

    // didn't find it; create and insert
    nsCOMPtr<nsScreenCocoa> sc = new nsScreenCocoa(screen);
    mScreenList.AppendObject(sc);
    return sc.get();
}

NS_IMETHODIMP
nsScreenManagerCocoa::ScreenForRect (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                                     nsIScreen **outScreen)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    NSEnumerator *screenEnum = [[NSScreen screens] objectEnumerator];
    NSRect inRect = nsCocoaUtils::GeckoRectToCocoaRect(nsIntRect(aX, aY, aWidth, aHeight));
    NSScreen *screenWindowIsOn = [NSScreen mainScreen];
    float greatestArea = 0;

    while (NSScreen *screen = [screenEnum nextObject]) {
        NSDictionary *desc = [screen deviceDescription];
        if ([desc objectForKey:NSDeviceIsScreen] == nil)
            continue;

        NSRect r = NSIntersectionRect([screen frame], inRect);
        float area = r.size.width * r.size.height;
        if (area > greatestArea) {
            greatestArea = area;
            screenWindowIsOn = screen;
        }
    }

    *outScreen = ScreenForCocoaScreen(screenWindowIsOn);
    NS_ADDREF(*outScreen);
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsScreenManagerCocoa::GetPrimaryScreen (nsIScreen **outScreen)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    // the mainScreen is the screen with the "key window" (focus, I assume?)
    NSScreen *sc = [[NSScreen screens] objectAtIndex:0];

    *outScreen = ScreenForCocoaScreen(sc);
    NS_ADDREF(*outScreen);

    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsScreenManagerCocoa::GetNumberOfScreens (PRUint32 *aNumberOfScreens)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    NSArray *ss = [NSScreen screens];

    *aNumberOfScreens = [ss count];

    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsScreenManagerCocoa::ScreenForNativeWidget (void *nativeWidget, nsIScreen **outScreen)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    NSWindow *window = static_cast<NSWindow*>(nativeWidget);
    if (window) {
        nsIScreen *screen = ScreenForCocoaScreen([window screen]);
        *outScreen = screen;
        NS_ADDREF(*outScreen);
        return NS_OK;
    }

    *outScreen = nsnull;
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
