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

class HeadlessLookAndFeel: public nsXPLookAndFeel {
public:
  HeadlessLookAndFeel();
  virtual ~HeadlessLookAndFeel();

  virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult);
  virtual nsresult GetIntImpl(IntID aID, int32_t &aResult);
  virtual nsresult GetFloatImpl(FloatID aID, float &aResult);
  virtual bool GetFontImpl(FontID aID,
                           nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel);

  virtual void RefreshImpl();
  virtual char16_t GetPasswordCharacterImpl();
  virtual bool GetEchoPasswordImpl();
};

} // namespace widget
} // namespace mozilla

#endif
