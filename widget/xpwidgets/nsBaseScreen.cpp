/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_FATAL_ASSERTIONS_FOR_THREAD_SAFETY

#include "nsBaseScreen.h"

NS_IMPL_ISUPPORTS1(nsBaseScreen, nsIScreen)

nsBaseScreen::nsBaseScreen()
{
  for (PRUint32 i = 0; i < nsIScreen::BRIGHTNESS_LEVELS; i++)
    mBrightnessLocks[i] = 0;
}

nsBaseScreen::~nsBaseScreen() { }

NS_IMETHODIMP
nsBaseScreen::LockMinimumBrightness(PRUint32 aBrightness)
{
  NS_ABORT_IF_FALSE(
    aBrightness < nsIScreen::BRIGHTNESS_LEVELS,
    "Invalid brightness level to lock");
  mBrightnessLocks[aBrightness]++;
  NS_ABORT_IF_FALSE(mBrightnessLocks[aBrightness] > 0,
    "Overflow after locking brightness level");

  CheckMinimumBrightness();

  return NS_OK;
}

NS_IMETHODIMP
nsBaseScreen::UnlockMinimumBrightness(PRUint32 aBrightness)
{
  NS_ABORT_IF_FALSE(
    aBrightness < nsIScreen::BRIGHTNESS_LEVELS,
    "Invalid brightness level to lock");
  NS_ABORT_IF_FALSE(mBrightnessLocks[aBrightness] > 0,
    "Unlocking a brightness level with no corresponding lock");
  mBrightnessLocks[aBrightness]--;

  CheckMinimumBrightness();

  return NS_OK;
}

void
nsBaseScreen::CheckMinimumBrightness()
{
  PRUint32 brightness = nsIScreen::BRIGHTNESS_LEVELS;
  for (PRUint32 i = 0; i < nsIScreen::BRIGHTNESS_LEVELS; i++)
    if (mBrightnessLocks[i] > 0)
      brightness = i;

  ApplyMinimumBrightness(brightness);
}
