/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.AppAndSystemHelper.isExternalAppBrowserActivityInCurrentTask
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName

class PwaRobot {
    fun verifyCustomTabToolbarIsNotDisplayed() = assertUIObjectExists(customTabToolbar(), exists = false)
    fun verifyPwaActivityInCurrentTask() = assertTrue(isExternalAppBrowserActivityInCurrentTask())

    class Transition
}

fun pwaScreen(interact: PwaRobot.() -> Unit): PwaRobot.Transition {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
    PwaRobot().interact()
    return PwaRobot.Transition()
}

private fun customTabToolbar() = mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
