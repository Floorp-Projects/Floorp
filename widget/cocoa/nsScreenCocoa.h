/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenCocoa_h_
#define nsScreenCocoa_h_

#import <Cocoa/Cocoa.h>

#include "nsBaseScreen.h"

class nsScreenCocoa : public nsBaseScreen
{
public:
    nsScreenCocoa (NSScreen *screen);
    ~nsScreenCocoa ();

    NS_IMETHOD GetRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
    NS_IMETHOD GetAvailRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
    NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth);
    NS_IMETHOD GetColorDepth(PRInt32* aColorDepth);

    NSScreen *CocoaScreen() { return mScreen; }

private:
    NSScreen *mScreen;
};

#endif // nsScreenCocoa_h_
