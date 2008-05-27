/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Frank Tang <ftang@netscape.com>
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

#ifndef nsIKBStateControl_h__
#define nsIKBStateControl_h__

#include "nsISupports.h"

// {AC4EBF71-86B6-4dab-A7FD-177501ADEC98}
#define NS_IKBSTATECONTROL_IID \
{ 0xac4ebf71, 0x86b6, 0x4dab, \
{ 0xa7, 0xfd, 0x17, 0x75, 0x01, 0xad, 0xec, 0x98 } }


/**
 * interface to control keyboard input state
 */
class nsIKBStateControl : public nsISupports {

  public:

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IKBSTATECONTROL_IID)

    /*
     * Force Input Method Editor to commit the uncommited input
     */
    NS_IMETHOD ResetInputState()=0;

    /*
     * Following methods relates to IME 'Opened'/'Closed' state.
     * 'Opened' means the user can input any character. I.e., users can input Japanese  
     * and other characters. The user can change the state to 'Closed'.
     * 'Closed' means the user can input ASCII characters only. This is the same as a
     * non-IME environment. The user can change the state to 'Opened'.
     * For more information is here.
     * http://bugzilla.mozilla.org/show_bug.cgi?id=16940#c48
     */

    /*
     * Set the state to 'Opened' or 'Closed'.
     * If aState is TRUE, IME open state is set to 'Opened'.
     * If aState is FALSE, set to 'Closed'.
     */
    NS_IMETHOD SetIMEOpenState(PRBool aState) = 0;

    /*
     * Get IME is 'Opened' or 'Closed'.
     * If IME is 'Opened', aState is set PR_TRUE.
     * If IME is 'Closed', aState is set PR_FALSE.
     */
    NS_IMETHOD GetIMEOpenState(PRBool* aState) = 0;

    /*
     * IME enabled states, the aState value of SetIMEEnabled/GetIMEEnabled
     * should be one value of following values.
     */
    enum {
      /*
       * 'Disabled' means the user cannot use IME. So, the open state should be
       * 'closed' during 'disabled'.
       */
      IME_STATUS_DISABLED = 0,
      /*
       * 'Enabled' means the user can use IME.
       */
      IME_STATUS_ENABLED = 1,
      /*
       * 'Password' state is a special case for the password editors.
       * E.g., on mac, the password editors should disable the non-Roman
       * keyboard layouts at getting focus. Thus, the password editor may have
       * special rules on some platforms.
       */
      IME_STATUS_PASSWORD = 2
    };

    /*
     * Set the state to 'Enabled' or 'Disabled' or 'Password'.
     */
    NS_IMETHOD SetIMEEnabled(PRUint32 aState) = 0;

    /*
     * Get IME is 'Enabled' or 'Disabled' or 'Password'.
     */
    NS_IMETHOD GetIMEEnabled(PRUint32* aState) = 0;

    /*
     * Destruct and don't commit the IME composition string.
     */
    NS_IMETHOD CancelIMEComposition() = 0;

    /*
     * Get toggled key states.
     * aKeyCode should be NS_VK_CAPS_LOCK or  NS_VK_NUM_LOCK or
     * NS_VK_SCROLL_LOCK.
     * aLEDState is the result for current LED state of the key.
     * If the LED is 'ON', it returns TRUE, otherwise, FALSE.
     * If the platform doesn't support the LED state (or we cannot get the
     * state), this method returns NS_ERROR_NOT_IMPLEMENTED.
     */
    NS_IMETHOD GetToggledKeyState(PRUint32 aKeyCode, PRBool* aLEDState) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIKBStateControl, NS_IKBSTATECONTROL_IID)

#endif // nsIKBStateControl_h__
