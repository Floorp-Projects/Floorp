package org.mozilla.fenix.ui.robots

import android.content.res.Resources
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.ext.waitNotNull

class NotificationRobot {

    fun verifySystemNotificationExists(notificationMessage: String) {

        fun notificationTray() = UiScrollable(
            UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller")
        )

        mDevice.waitNotNull(
            Until.hasObject(text(notificationMessage)),
            waitingTime
        )

        var notificationFound = false
        while (!notificationFound) {
            try {
                val notification = notificationTray().getChildByText(
                    UiSelector().text(notificationMessage), notificationMessage,
                    true
                )
                notification.exists()
                notificationFound = true
            } catch (e: Resources.NotFoundException) {
                e.printStackTrace()
            }
        }
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
