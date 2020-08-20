package org.mozilla.fenix.ui.robots

import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.ext.waitNotNull

class NotificationRobot {

    fun verifySystemNotificationExists(notificationMessage: String) {

        fun notificationTray() = UiScrollable(
            UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller")
        )

        val notificationFound: Boolean

        notificationFound = try {
            notificationTray().getChildByText(
                UiSelector().text(notificationMessage), notificationMessage, true
            ).exists()
        } catch (e: UiObjectNotFoundException) {
            false
        }

        if (!notificationFound) {
            // swipe 2 times to expand the silent notifications on API 28 and higher, single-swipe doesn't do it
            notificationTray().swipeUp(2)
            val notification = mDevice.findObject(UiSelector().textContains(notificationMessage))
            assertTrue(notification.exists())
        }
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

    fun clickMediaSystemNotificationControlButton(action: String) {
        mediaSystemNotificationButton(action).waitForExists(waitingTime)
        mediaSystemNotificationButton(action).click()
    }

    fun verifyMediaSystemNotificationButtonState(action: String) {
        mediaSystemNotificationButton(action).waitForExists(waitingTime)
    }

    class Transition {

        fun clickClosePrivateTabsNotification(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            NotificationRobot().verifySystemNotificationExists("Close private tabs")
            closePrivateTabsNotification().clickAndWaitForNewWindow()

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
