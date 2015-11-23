/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <UIKit/UIScreen.h>

#include "gfxPoint.h"
#include "nsScreenManager.h"
#include "nsAppShell.h"

static LayoutDeviceIntRect gScreenBounds;
static bool gScreenBoundsSet = false;

UIKitScreen::UIKitScreen(UIScreen* aScreen)
{
    mScreen = [aScreen retain];
}

NS_IMETHODIMP
UIKitScreen::GetRect(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
    return GetRectDisplayPix(outX, outY, outWidth, outHeight);
}

NS_IMETHODIMP
UIKitScreen::GetAvailRect(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
    return GetAvailRectDisplayPix(outX, outY, outWidth, outHeight);
}

NS_IMETHODIMP
UIKitScreen::GetRectDisplayPix(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
    nsIntRect rect = UIKitScreenManager::GetBounds();
    *outX = rect.x;
    *outY = rect.y;
    *outWidth = rect.width;
    *outHeight = rect.height;

    return NS_OK;
}

NS_IMETHODIMP
UIKitScreen::GetAvailRectDisplayPix(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
    CGRect rect = [mScreen applicationFrame];
    CGFloat scale = [mScreen scale];

    *outX = rect.origin.x * scale;
    *outY = rect.origin.y * scale;
    *outWidth = rect.size.width * scale;
    *outHeight = rect.size.height * scale;

    return NS_OK;
}

NS_IMETHODIMP
UIKitScreen::GetPixelDepth(int32_t *aPixelDepth)
{
  // Close enough.
  *aPixelDepth = 24;
  return NS_OK;
}

NS_IMETHODIMP
UIKitScreen::GetColorDepth(int32_t *aColorDepth)
{
  return GetPixelDepth(aColorDepth);
}

NS_IMETHODIMP
UIKitScreen::GetContentsScaleFactor(double* aContentsScaleFactor)
{
    *aContentsScaleFactor = [mScreen scale];
    return NS_OK;
}

NS_IMPL_ISUPPORTS(UIKitScreenManager, nsIScreenManager)

UIKitScreenManager::UIKitScreenManager()
: mScreen(new UIKitScreen([UIScreen mainScreen]))
{
}

LayoutDeviceIntRect
UIKitScreenManager::GetBounds()
{
    if (!gScreenBoundsSet) {
        CGRect rect = [[UIScreen mainScreen] bounds];
        CGFloat scale = [[UIScreen mainScreen] scale];
        gScreenBounds.x = rect.origin.x * scale;
        gScreenBounds.y = rect.origin.y * scale;
        gScreenBounds.width = rect.size.width * scale;
        gScreenBounds.height = rect.size.height * scale;
        gScreenBoundsSet = true;
    }
    printf("UIKitScreenManager::GetBounds: %d %d %d %d\n",
           gScreenBounds.x, gScreenBounds.y, gScreenBounds.width, gScreenBounds.height);
    return gScreenBounds;
}

NS_IMETHODIMP
UIKitScreenManager::GetPrimaryScreen(nsIScreen** outScreen)
{
  NS_IF_ADDREF(*outScreen = mScreen.get());
  return NS_OK;
}

NS_IMETHODIMP
UIKitScreenManager::ScreenForRect(int32_t inLeft,
                               int32_t inTop,
                               int32_t inWidth,
                               int32_t inHeight,
                               nsIScreen** outScreen)
{
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
UIKitScreenManager::ScreenForId(uint32_t id,
                                nsIScreen** outScreen)
{
    return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
UIKitScreenManager::ScreenForNativeWidget(void* aWidget, nsIScreen** outScreen)
{
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
UIKitScreenManager::GetNumberOfScreens(uint32_t* aNumberOfScreens)
{
  //TODO: support multiple screens
  *aNumberOfScreens = 1;
  return NS_OK;
}

NS_IMETHODIMP
UIKitScreenManager::GetSystemDefaultScale(float* aScale)
{
    *aScale = [UIScreen mainScreen].scale;
    return NS_OK;
}
