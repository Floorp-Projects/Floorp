/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_FATAL_ASSERTIONS_FOR_THREAD_SAFETY

#include "nsBaseScreen.h"

NS_IMPL_ISUPPORTS(nsBaseScreen, nsIScreen)

nsBaseScreen::nsBaseScreen()
{
  for (uint32_t i = 0; i < nsIScreen::BRIGHTNESS_LEVELS; i++)
    mBrightnessLocks[i] = 0;
}

nsBaseScreen::~nsBaseScreen() { }

NS_IMETHODIMP
nsBaseScreen::GetRectDisplayPix(int32_t *outLeft,  int32_t *outTop,
                                int32_t *outWidth, int32_t *outHeight)
{
  return GetRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
nsBaseScreen::GetAvailRectDisplayPix(int32_t *outLeft,  int32_t *outTop,
                                     int32_t *outWidth, int32_t *outHeight)
{
  return GetAvailRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
nsBaseScreen::LockMinimumBrightness(uint32_t aBrightness)
{
  MOZ_ASSERT(aBrightness < nsIScreen::BRIGHTNESS_LEVELS,
             "Invalid brightness level to lock");
  mBrightnessLocks[aBrightness]++;
  MOZ_ASSERT(mBrightnessLocks[aBrightness] > 0,
             "Overflow after locking brightness level");

  CheckMinimumBrightness();

  return NS_OK;
}

NS_IMETHODIMP
nsBaseScreen::UnlockMinimumBrightness(uint32_t aBrightness)
{
  MOZ_ASSERT(aBrightness < nsIScreen::BRIGHTNESS_LEVELS,
             "Invalid brightness level to lock");
  MOZ_ASSERT(mBrightnessLocks[aBrightness] > 0,
             "Unlocking a brightness level with no corresponding lock");
  mBrightnessLocks[aBrightness]--;

  CheckMinimumBrightness();

  return NS_OK;
}

void
nsBaseScreen::CheckMinimumBrightness()
{
  uint32_t brightness = nsIScreen::BRIGHTNESS_LEVELS;
  for (int32_t i = nsIScreen::BRIGHTNESS_LEVELS - 1; i >=0; i--) {
    if (mBrightnessLocks[i] > 0) {
      brightness = i;
      break;
    }
  }

  ApplyMinimumBrightness(brightness);
}

NS_IMETHODIMP
nsBaseScreen::GetContentsScaleFactor(double* aContentsScaleFactor)
{
  *aContentsScaleFactor = 1.0;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseScreen::GetDefaultCSSScaleFactor(double* aScaleFactor)
{
  *aScaleFactor = 1.0;
  return NS_OK;
}
