/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import android.util.Log
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.mDevice

class NotificationRobot {
    fun verifySystemNotificationExists(notificationMessage: String) {
        var notificationTray =
            mDevice.findObject(
                UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller")
            )

        if (notificationTray.isScrollable) {
            notificationTray = UiScrollable(
                UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller")
            )

            var notificationFound = false

            do {
                try {
                    notificationFound = notificationTray.getChildByText(
                        UiSelector().text(notificationMessage), notificationMessage, true
                    ).waitForExists(TestHelper.waitingTime)
                    assertTrue(notificationFound)
                } catch (e: UiObjectNotFoundException) {
                    Log.d("TestLog", e.message.toString())
                    // scrolls down the notifications until it reaches the end, then it will close
                    notificationTray.scrollForward()
                    mDevice.waitForIdle()
                }
            } while (!notificationFound)
        } else {
            val notificationFound =
                notificationTray.getChild(UiSelector().textContains(notificationMessage)).exists()
            assertTrue(notificationFound)
        }
    }
}
