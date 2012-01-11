/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
   *                    nsIScreen_MOZILLA_2_0_BRANCH::BRIGHTNESS_LEVELS
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
