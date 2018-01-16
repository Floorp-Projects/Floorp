/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "nsXPLookAndFeel.h"
#include "nsCOMPtr.h"
#include "gfxFont.h"

struct _GtkStyle;

class nsLookAndFeel final : public nsXPLookAndFeel
{
public:
    nsLookAndFeel();
    virtual ~nsLookAndFeel();

    virtual void NativeInit() final override;
    virtual void RefreshImpl() override;
    virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult) override;
    virtual nsresult GetIntImpl(IntID aID, int32_t &aResult) override;
    virtual nsresult GetFloatImpl(FloatID aID, float &aResult) override;
    virtual bool GetFontImpl(FontID aID, nsString& aFontName,
                             gfxFontStyle& aFontStyle,
                             float aDevPixPerCSSPixel) override;

    virtual char16_t GetPasswordCharacterImpl() override;
    virtual bool GetEchoPasswordImpl() override;

    bool IsCSDAvailable() const { return mCSDAvailable; }

protected:

    // Cached fonts
    bool mDefaultFontCached;
    bool mButtonFontCached;
    bool mFieldFontCached;
    bool mMenuFontCached;
    nsString mDefaultFontName;
    nsString mButtonFontName;
    nsString mFieldFontName;
    nsString mMenuFontName;
    gfxFontStyle mDefaultFontStyle;
    gfxFontStyle mButtonFontStyle;
    gfxFontStyle mFieldFontStyle;
    gfxFontStyle mMenuFontStyle;

    // Cached colors
    nscolor mInfoBackground;
    nscolor mInfoText;
    nscolor mMenuBackground;
    nscolor mMenuBarText;
    nscolor mMenuBarHoverText;
    nscolor mMenuText;
    nscolor mMenuTextInactive;
    nscolor mMenuHover;
    nscolor mMenuHoverText;
    nscolor mButtonDefault;
    nscolor mButtonText;
    nscolor mButtonHoverText;
    nscolor mButtonHoverFace;
    nscolor mFrameOuterLightBorder;
    nscolor mFrameInnerDarkBorder;
    nscolor mOddCellBackground;
    nscolor mNativeHyperLinkText;
    nscolor mComboBoxText;
    nscolor mComboBoxBackground;
    nscolor mMozFieldText;
    nscolor mMozFieldBackground;
    nscolor mMozWindowText;
    nscolor mMozWindowBackground;
    nscolor mMozWindowActiveBorder;
    nscolor mMozWindowInactiveBorder;
    nscolor mMozWindowInactiveCaption;
    nscolor mTextSelectedText;
    nscolor mTextSelectedBackground;
    nscolor mMozScrollbar;
    nscolor mInfoBarText;
    char16_t mInvisibleCharacter;
    float   mCaretRatio;
    bool    mMenuSupportsDrag;
    bool    mCSDAvailable;
    bool    mCSDMaximizeButton;
    bool    mCSDMinimizeButton;
    bool    mCSDCloseButton;
    bool    mInitialized;

    void EnsureInit();
};

#endif
