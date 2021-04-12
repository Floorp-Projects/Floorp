/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import android.util.Log
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class NotificationRobot {

    fun clearNotifications() {
        if (clearButton.exists()) {
            clearButton.click()
        } else {
            notificationTray.flingToEnd(3)
            if (clearButton.exists()) {
                clearButton.click()
            } else if (notificationTray.exists()) {
                mDevice.pressBack()
            }
        }
    }

    fun expandEraseBrowsingNotification() {
        val PINCH_PERCENT = 50
        val PINCH_STEPS = 5

        if (!notificationEraseAndOpenButton.waitForExists(waitingTime)) {
            if (!notificationTray.ensureFullyVisible(notificationHeader)) {
                eraseBrowsingNotification.pinchOut(
                    PINCH_PERCENT,
                    PINCH_STEPS
                )
            } else {
                notificationHeader.click()
            }
            notificationTray.ensureFullyVisible(notificationEraseAndOpenButton)
            assertTrue(notificationEraseAndOpenButton.exists())
        }
    }

    fun verifySystemNotificationExists(notificationMessage: String) {
        var notificationFound = false

        do {
            try {
                notificationFound = notificationTray.getChildByText(
                    UiSelector().text(notificationMessage), notificationMessage, true
                ).waitForExists(waitingTime)
                assertTrue(notificationFound)
            } catch (e: UiObjectNotFoundException) {
                Log.d("TestLog", e.message.toString())
                // scrolls down the notifications until it reaches the end, then it will close
                notificationTray.scrollForward()
                mDevice.waitForIdle()
            }
        } while (!notificationFound)
    }

    class Transition {
        fun clickEraseAndOpenNotificationButton(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            notificationEraseAndOpenButton.waitForExists(TestHelper.waitingTime)
            notificationEraseAndOpenButton.click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun clickNotificationOpenButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            notificationOpenButton.waitForExists(TestHelper.waitingTime)
            notificationOpenButton.clickAndWaitForNewWindow()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun notificationTray(interact: NotificationRobot.() -> Unit): NotificationRobot.Transition {
    NotificationRobot().interact()
    return NotificationRobot.Transition()
}

private val eraseBrowsingNotification =
    mDevice.findObject(
        UiSelector().text(getStringResource(R.string.notification_erase_text))
    )

private val notificationEraseAndOpenButton =
    mDevice.findObject(
        UiSelector().description(getStringResource(R.string.notification_action_erase_and_open))
    )

private val notificationOpenButton = mDevice.findObject(
    UiSelector().description(getStringResource(R.string.notification_action_open))
)

private val notificationTray = UiScrollable(
    UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller")
).setAsVerticalList()

private val appName = InstrumentationRegistry.getInstrumentation().targetContext.getString(R.string.app_name)

private val notificationHeader = mDevice.findObject(
    UiSelector()
        .resourceId("android:id/app_name_text")
        .textContains(appName)
)

private val clearButton = mDevice.findObject(UiSelector().resourceId("com.android.systemui:id/dismiss_text"))
