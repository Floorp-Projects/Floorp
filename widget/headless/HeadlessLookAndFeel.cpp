/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessLookAndFeel.h"

using mozilla::LookAndFeel;

namespace mozilla {
namespace widget {

static const char16_t UNICODE_BULLET = 0x2022;

HeadlessLookAndFeel::HeadlessLookAndFeel()
{
}

HeadlessLookAndFeel::~HeadlessLookAndFeel()
{
}

nsresult
HeadlessLookAndFeel::NativeGetColor(ColorID aID, nscolor& aColor)
{
  // Default all colors to black.
  aColor = NS_RGB(0x00, 0x00, 0x00);
  return NS_OK;
}

nsresult
HeadlessLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) {
    return res;
  }
  aResult = 0;
  return NS_ERROR_FAILURE;
}

nsresult
HeadlessLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
  nsresult res = NS_OK;
  res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) {
    return res;
  }
  aResult = -1.0;
  return NS_ERROR_FAILURE;
}

bool
HeadlessLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
               gfxFontStyle& aFontStyle,
               float aDevPixPerCSSPixel)
{
  return true;
}

char16_t
HeadlessLookAndFeel::GetPasswordCharacterImpl()
{
  return UNICODE_BULLET;
}

void
HeadlessLookAndFeel::RefreshImpl()
{
  nsXPLookAndFeel::RefreshImpl();
}

bool
HeadlessLookAndFeel::GetEchoPasswordImpl() {
  return false;
}

} // namespace widget
} // namespace mozilla
