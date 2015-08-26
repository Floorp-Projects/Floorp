/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_FATAL_ASSERTIONS_FOR_THREAD_SAFETY

#include "nsScreenManagerAndroid.h"
#include "nsWindow.h"
#include "AndroidBridge.h"
#include "GeneratedJNIWrappers.h"
#include "AndroidRect.h"
#include <mozilla/jni/Refs.h>

using namespace mozilla;

nsScreenAndroid::nsScreenAndroid(void *nativeScreen)
{
}

nsScreenAndroid::~nsScreenAndroid()
{
}

NS_IMETHODIMP
nsScreenAndroid::GetId(uint32_t *outId)
{
    *outId = 1;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenAndroid::GetRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
    widget::sdk::Rect::LocalRef rect = widget::GeckoAppShell::GetScreenSize();
    rect->Left(outLeft);
    rect->Top(outTop);
    rect->Width(outWidth);
    rect->Height(outHeight);

    return NS_OK;
}


NS_IMETHODIMP
nsScreenAndroid::GetAvailRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
    return GetRect(outLeft, outTop, outWidth, outHeight);
}



NS_IMETHODIMP
nsScreenAndroid::GetPixelDepth(int32_t *aPixelDepth)
{
    *aPixelDepth = AndroidBridge::Bridge()->GetScreenDepth();
    return NS_OK;
}


NS_IMETHODIMP
nsScreenAndroid::GetColorDepth(int32_t *aColorDepth)
{
    return GetPixelDepth(aColorDepth);
}

void
nsScreenAndroid::ApplyMinimumBrightness(uint32_t aBrightness)
{
    widget::GeckoAppShell::SetKeepScreenOn(aBrightness == BRIGHTNESS_FULL);
}

NS_IMPL_ISUPPORTS(nsScreenManagerAndroid, nsIScreenManager)

nsScreenManagerAndroid::nsScreenManagerAndroid()
{
    mOneScreen = new nsScreenAndroid(nullptr);
}

nsScreenManagerAndroid::~nsScreenManagerAndroid()
{
}

NS_IMETHODIMP
nsScreenManagerAndroid::GetPrimaryScreen(nsIScreen **outScreen)
{
    NS_IF_ADDREF(*outScreen = mOneScreen.get());
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerAndroid::ScreenForId(uint32_t aId,
                                    nsIScreen **outScreen)
{
    return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManagerAndroid::ScreenForRect(int32_t inLeft,
                                      int32_t inTop,
                                      int32_t inWidth,
                                      int32_t inHeight,
                                      nsIScreen **outScreen)
{
    return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManagerAndroid::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
    return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManagerAndroid::GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
    *aNumberOfScreens = 1;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerAndroid::GetSystemDefaultScale(float *aDefaultScale)
{
    *aDefaultScale = 1.0f;
    return NS_OK;
}
