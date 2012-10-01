/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "nsXPLookAndFeel.h"
#include "AndroidBridge.h"

class nsLookAndFeel: public nsXPLookAndFeel
{
public:
    nsLookAndFeel();
    virtual ~nsLookAndFeel();

    virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult);
    virtual nsresult GetIntImpl(IntID aID, int32_t &aResult);
    virtual nsresult GetFloatImpl(FloatID aID, float &aResult);
    virtual bool GetFontImpl(FontID aID, nsString& aName, gfxFontStyle& aStyle,
                             float aDevPixPerCSSPixel);
    virtual bool GetEchoPasswordImpl();
    virtual uint32_t GetPasswordMaskDelayImpl();
    virtual PRUnichar GetPasswordCharacterImpl();

protected:
    static bool mInitializedSystemColors;
    static mozilla::AndroidSystemColors mSystemColors;
    static bool mInitializedShowPassword;
    static bool mShowPassword;

    nsresult GetSystemColors();
    nsresult CallRemoteGetSystemColors();
};

#endif
