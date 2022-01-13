/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.ext.waitNotNull

class NotificationRobot {

    fun verifySystemNotificationExists(notificationMessage: String) {
        var notificationFound = false

        do {
            try {
                notificationFound = notificationTray().getChildByText(
                    UiSelector().text(notificationMessage), notificationMessage, true
                ).waitForExists(waitingTime)
                assertTrue(notificationFound)
            } catch (e: UiObjectNotFoundException) {
                notificationTray().scrollForward()
                mDevice.waitForIdle()
            }
        } while (!notificationFound)
    }

    fun verifySystemNotificationGone(notificationMessage: String) {
        mDevice.waitNotNull(
            Until.gone(text(notificationMessage)),
            waitingTime
        )

        assertFalse(
            mDevice.findObject(
                UiSelector().text(notificationMessage)
            ).exists()
        )
    }

    fun verifyPrivateTabsNotification() {
        mDevice.waitNotNull(Until.hasObject(text("Close private tabs")), waitingTime)
        assertPrivateTabsNotification()
    }

    fun clickSystemNotificationControlButton(action: String) {
        mediaSystemNotificationButton(action).waitForExists(waitingTime)
        mediaSystemNotificationButton(action).click()
    }

    fun verifyMediaSystemNotificationButtonState(action: String) {
        assertTrue(mediaSystemNotificationButton(action).waitForExists(waitingTime))
    }

    fun expandNotificationMessage() {
        while (!notificationHeader.exists()) {
            notificationTray().scrollForward()
        }
        notificationHeader.click()
    }

    class Transition {

        fun clickClosePrivateTabsNotification(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            NotificationRobot().verifySystemNotificationExists("Close private tabs")
            closePrivateTabsNotification().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun notificationShade(interact: NotificationRobot.() -> Unit): NotificationRobot.Transition {
    NotificationRobot().interact()
    return NotificationRobot.Transition()
}

private fun assertPrivateTabsNotification() {
    mDevice.findObject(UiSelector().text("Firefox Preview (Private)")).exists()
    mDevice.findObject(UiSelector().text("Close private tabs")).exists()
}

private fun closePrivateTabsNotification() =
    mDevice.findObject(UiSelector().text("Close private tabs"))

private fun mediaSystemNotificationButton(action: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("android:id/action0")
            .descriptionContains(action)
    )

private fun notificationTray() = UiScrollable(
    UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller")
)

private val notificationHeader =
    mDevice.findObject(
        UiSelector()
            .resourceId("android:id/app_name_text")
            .text(appName)
    )
