/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "nsXPLookAndFeel.h"

class nsLookAndFeel final : public nsXPLookAndFeel
{
public:
    nsLookAndFeel();
    virtual ~nsLookAndFeel();

    virtual nsresult NativeGetColor(const ColorID aID, nscolor &aResult);
    virtual nsresult GetIntImpl(IntID aID, int32_t &aResult);
    virtual nsresult GetFloatImpl(FloatID aID, float &aResult);
    virtual bool GetFontImpl(FontID aID, nsString& aFontName,
                             gfxFontStyle& aFontStyle,
                             float aDevPixPerCSSPixel);
    virtual char16_t GetPasswordCharacterImpl()
    {
        // unicode value for the bullet character, used for password textfields.
        return 0x2022;
    }

    static bool UseOverlayScrollbars()
    {
        return true;
    }
};

#endif
