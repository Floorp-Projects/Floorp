/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_HeadlessLookAndFeel_h
#define mozilla_widget_HeadlessLookAndFeel_h

#include "nsXPLookAndFeel.h"
#include "nsLookAndFeel.h"

namespace mozilla {
namespace widget {

#if defined(MOZ_WIDGET_GTK)

// Our nsLookAndFeel for GTK relies on APIs that aren't available in headless
// mode, so we use an implementation with hardcoded values.

class HeadlessLookAndFeel: public nsXPLookAndFeel {
public:
  HeadlessLookAndFeel();
  virtual ~HeadlessLookAndFeel();

  virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult) override;
  virtual void NativeInit() final override {};
  virtual nsresult GetIntImpl(IntID aID, int32_t &aResult) override;
  virtual nsresult GetFloatImpl(FloatID aID, float &aResult) override;
  virtual bool GetFontImpl(FontID aID,
                           nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel) override;

  virtual void RefreshImpl() override;
  virtual char16_t GetPasswordCharacterImpl() override;
  virtual bool GetEchoPasswordImpl() override;
};

#else

// When possible, we simply reuse the platform's existing nsLookAndFeel
// implementation in headless mode.

typedef nsLookAndFeel HeadlessLookAndFeel;

#endif

} // namespace widget
} // namespace mozilla

#endif
