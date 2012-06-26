/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseScreen_h
#define nsBaseScreen_h

#include "mozilla/Attributes.h"
#include "nsIScreen.h"

class nsBaseScreen : public nsIScreen
{
public:
  nsBaseScreen();
  virtual ~nsBaseScreen();
  
  NS_DECL_ISUPPORTS

  // nsIScreen interface

  /**
   * Simple management of screen brightness locks. This abstract base class
   * allows all widget implementations to share brightness locking code.
   */
  NS_IMETHOD LockMinimumBrightness(PRUint32 aBrightness);
  NS_IMETHOD UnlockMinimumBrightness(PRUint32 aBrightness);

  NS_IMETHOD GetRotation(PRUint32* aRotation) {
    *aRotation = nsIScreen::ROTATION_0_DEG;
    return NS_OK;
  }
  NS_IMETHOD SetRotation(PRUint32 aRotation) { return NS_ERROR_NOT_AVAILABLE; }

protected:
  /**
   * Manually set the current level of brightness locking. This is called after
   * we determine, based on the current active locks, what the strongest
   * lock is. You should normally not call this function - it will be
   * called automatically by this class.
   *
   * Each widget implementation should implement this in a way that
   * makes sense there. This is normally the only function that
   * contains widget-specific code.
   *
   * The default implementation does nothing.
   *
   * @param aBrightness The current brightness level to set. If this is
   *                    nsIScreen::BRIGHTNESS_LEVELS
   *                    (an impossible value for a brightness level to be),
   *                    then that signifies that there is no current
   *                    minimum brightness level, and the screen can shut off.
   */
  virtual void ApplyMinimumBrightness(PRUint32 aBrightness) { }

private:
  /**
   * Checks what the minimum brightness value is, and calls
   * ApplyMinimumBrightness.
   */
  void CheckMinimumBrightness();

  PRUint32 mBrightnessLocks[nsIScreen::BRIGHTNESS_LEVELS];
};

#endif // nsBaseScreen_h
