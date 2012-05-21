/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerAndroid_h___
#define nsScreenManagerAndroid_h___

#include "nsCOMPtr.h"

#include "nsBaseScreen.h"
#include "nsIScreenManager.h"
#include "WidgetUtils.h"

class nsScreenAndroid : public nsBaseScreen
{
public:
    nsScreenAndroid(void *nativeScreen);
    ~nsScreenAndroid();

    NS_IMETHOD GetRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
    NS_IMETHOD GetAvailRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight);
    NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth);
    NS_IMETHOD GetColorDepth(PRInt32* aColorDepth);

protected:
    virtual void ApplyMinimumBrightness(PRUint32 aBrightness) MOZ_OVERRIDE;
};

class nsScreenManagerAndroid :
    public nsIScreenManager
{
public:
    nsScreenManagerAndroid();
    ~nsScreenManagerAndroid();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCREENMANAGER

protected:
    nsCOMPtr<nsIScreen> mOneScreen;
};

#endif /* nsScreenManagerAndroid_h___ */
