/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScreenProxy_h
#define mozilla_widget_ScreenProxy_h

#include "nsBaseScreen.h"
#include "mozilla/dom/PScreenManagerChild.h"
#include "mozilla/dom/TabChild.h"

class nsScreenManagerProxy;

namespace mozilla {
namespace widget {

class ScreenProxy : public nsBaseScreen
{
public:
    ScreenProxy(nsScreenManagerProxy* aScreenManager,
                mozilla::dom::ScreenDetails aDetails);
    ~ScreenProxy() {};

    NS_IMETHOD GetId(uint32_t* aId) override;

    NS_IMETHOD GetRect(int32_t* aLeft,
                       int32_t* aTop,
                       int32_t* aWidth,
                       int32_t* aHeight) override;
    NS_IMETHOD GetRectDisplayPix(int32_t* aLeft,
                                 int32_t* aTop,
                                 int32_t* aWidth,
                                 int32_t* aHeight) override;
    NS_IMETHOD GetAvailRect(int32_t* aLeft,
                            int32_t* aTop,
                            int32_t* aWidth,
                            int32_t* aHeight) override;
    NS_IMETHOD GetAvailRectDisplayPix(int32_t* aLeft,
                                      int32_t* aTop,
                                      int32_t* aWidth,
                                      int32_t* aHeight) override;
    NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth) override;
    NS_IMETHOD GetColorDepth(int32_t* aColorDepth) override;

    NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor) override;
    NS_IMETHOD GetDefaultCSSScaleFactor(double* aScaleFactor) override;

private:

    void PopulateByDetails(mozilla::dom::ScreenDetails aDetails);
    bool EnsureCacheIsValid();
    void InvalidateCacheOnNextTick();
    void InvalidateCache();

    double mContentsScaleFactor;
    double mDefaultCSSScaleFactor;
    RefPtr<nsScreenManagerProxy> mScreenManager;
    uint32_t mId;
    int32_t mPixelDepth;
    int32_t mColorDepth;
    nsIntRect mRect;
    nsIntRect mRectDisplayPix;
    nsIntRect mAvailRect;
    nsIntRect mAvailRectDisplayPix;
    bool mCacheValid;
    bool mCacheWillInvalidate;
};

} // namespace widget
} // namespace mozilla

#endif

