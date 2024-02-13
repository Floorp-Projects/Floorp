/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.AppAndSystemHelper.isExternalAppBrowserActivityInCurrentTask
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName

class PwaRobot {
    fun verifyCustomTabToolbarIsNotDisplayed() = assertUIObjectExists(itemWithResId("$packageName:id/toolbar"), exists = false)
    fun verifyPwaActivityInCurrentTask() {
        assertTrue("$TAG: The latest activity of the application is not used for custom tabs or PWAs", isExternalAppBrowserActivityInCurrentTask())
    }

    class Transition
}

fun pwaScreen(interact: PwaRobot.() -> Unit): PwaRobot.Transition {
    Log.i(TAG, "pwaScreen: Trying to find the engine view")
    mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
    Log.i(Constants.TAG, "pwaScreen: Found the engine view")
    PwaRobot().interact()
    return PwaRobot.Transition()
}
