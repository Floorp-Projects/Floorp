/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerGonk_h___
#define nsScreenManagerGonk_h___

#include "mozilla/Hal.h"
#include "nsCOMPtr.h"

#include "nsBaseScreen.h"
#include "nsIScreenManager.h"

class nsScreenGonk : public nsBaseScreen
{
    typedef mozilla::hal::ScreenConfiguration ScreenConfiguration;

public:
    nsScreenGonk(void* nativeScreen);
    ~nsScreenGonk();

    NS_IMETHOD GetRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
    NS_IMETHOD GetAvailRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
    NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth);
    NS_IMETHOD GetColorDepth(PRInt32* aColorDepth);
    NS_IMETHOD GetRotation(PRUint32* aRotation);
    NS_IMETHOD SetRotation(PRUint32  aRotation);

    static uint32_t GetRotation();
    static ScreenConfiguration GetConfiguration();
};

class nsScreenManagerGonk : public nsIScreenManager
{
public:
    nsScreenManagerGonk();
    ~nsScreenManagerGonk();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCREENMANAGER

protected:
    nsCOMPtr<nsIScreen> mOneScreen;
};

#endif /* nsScreenManagerGonk_h___ */
