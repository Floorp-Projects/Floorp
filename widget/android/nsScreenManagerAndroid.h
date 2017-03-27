/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerAndroid_h___
#define nsScreenManagerAndroid_h___

#include "nsCOMPtr.h"

#include "nsBaseScreen.h"
#include "nsIScreenManager.h"
#include "nsRect.h"
#include "mozilla/WidgetUtils.h"

class nsScreenAndroid final : public nsBaseScreen
{
public:
    nsScreenAndroid(DisplayType aDisplayType, nsIntRect aRect);
    ~nsScreenAndroid();

    NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth) override;
    NS_IMETHOD GetColorDepth(int32_t* aColorDepth) override;

    uint32_t GetId() const { return mId; };
    DisplayType GetDisplayType() const { return mDisplayType; }

    void SetDensity(double aDensity) { mDensity = aDensity; }
    float GetDensity();

private:
    uint32_t mId;
    DisplayType mDisplayType;
    nsIntRect mRect;
    float mDensity;
};

class nsScreenManagerAndroid final : public nsIScreenManager
{
private:
    ~nsScreenManagerAndroid();

public:
    class ScreenManagerHelperSupport;

    nsScreenManagerAndroid();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCREENMANAGER

    already_AddRefed<nsScreenAndroid> ScreenForId(uint32_t aId);
    already_AddRefed<nsScreenAndroid> AddScreen(DisplayType aDisplayType,
                                                nsIntRect aRect = nsIntRect());
    void RemoveScreen(uint32_t aScreenId);

protected:
    nsTArray<RefPtr<nsScreenAndroid>> mScreens;
};

#endif /* nsScreenManagerAndroid_h___ */
