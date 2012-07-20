/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
