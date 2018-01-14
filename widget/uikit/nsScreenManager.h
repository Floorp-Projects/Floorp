/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManager_h_
#define nsScreenManager_h_

#include "nsBaseScreen.h"
#include "nsIScreenManager.h"
#include "nsCOMPtr.h"
#include "nsRect.h"

@class UIScreen;

class UIKitScreen : public nsBaseScreen
{
public:
    explicit UIKitScreen (UIScreen* screen);
    ~UIKitScreen () {}

    NS_IMETHOD GetId(uint32_t* outId) override {
        *outId = 0;
        return NS_OK;
    }

    NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetRectDisplayPix(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetAvailRectDisplayPix(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth) override;
    NS_IMETHOD GetColorDepth(int32_t* aColorDepth) override;
    NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor) override;
    NS_IMETHOD GetDefaultCSSScaleFactor(double* aScaleFactor) override
    {
      return GetContentsScaleFactor(aScaleFactor);
    }

private:
    UIScreen* mScreen;
};

class UIKitScreenManager : public nsIScreenManager
{
public:
    UIKitScreenManager ();

    NS_DECL_ISUPPORTS

    NS_DECL_NSISCREENMANAGER

    static LayoutDeviceIntRect GetBounds();

private:
    virtual ~UIKitScreenManager () {}
    //TODO: support >1 screen, iPad supports external displays
    nsCOMPtr<nsIScreen> mScreen;
};

#endif // nsScreenManager_h_
