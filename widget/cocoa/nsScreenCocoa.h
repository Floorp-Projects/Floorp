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

    NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
    NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
    NS_IMETHOD GetRectDisplayPix(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
    NS_IMETHOD GetAvailRectDisplayPix(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
    NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth);
    NS_IMETHOD GetColorDepth(int32_t* aColorDepth);
    NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor);

    NSScreen *CocoaScreen() { return mScreen; }

private:
    CGFloat BackingScaleFactor();

    NSScreen *mScreen;
};

#endif // nsScreenCocoa_h_
