/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
nsScreenManagerCocoa::ScreenForCocoaScreen(NSScreen *screen)
{
    for (PRUint32 i = 0; i < mScreenList.Length(); ++i) {
        nsScreenCocoa* sc = mScreenList[i];
        if (sc->CocoaScreen() == screen) {
            // doesn't addref
            return sc;
        }
    }

    // didn't find it; create and insert
    nsRefPtr<nsScreenCocoa> sc = new nsScreenCocoa(screen);
    mScreenList.AppendElement(sc);
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

    *outScreen = nullptr;
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
