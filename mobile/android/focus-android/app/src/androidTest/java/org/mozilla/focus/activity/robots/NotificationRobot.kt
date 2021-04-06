package org.mozilla.focus.activity.robots

import android.util.Log
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class NotificationRobot {

    fun expandEraseBrowsingNotification() {
        val PINCH_PERCENT = 50
        val PINCH_STEPS = 5

        val notificationExpandSwitch = mDevice.findObject(
            UiSelector()
                .resourceId("android:id/expand_button")
                .textContains("Firefox Focus")
        )
        if (!notificationEraseAndOpenButton.waitForExists(waitingTime)) {
            if (!notificationExpandSwitch.exists()) {
                eraseBrowsingNotification.pinchOut(
                    PINCH_PERCENT,
                    PINCH_STEPS
                )
            } else {
                notificationExpandSwitch.click()
            }
            assertTrue(notificationEraseAndOpenButton.exists())
        }
    }

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
        UiSelector().text(TestHelper.getStringResource(R.string.notification_erase_text))
    )

private val notificationEraseAndOpenButton =
    mDevice.findObject(
        UiSelector().description(TestHelper.getStringResource(R.string.notification_action_erase_and_open))
    )

private val notificationOpenButton = mDevice.findObject(
    UiSelector().description(TestHelper.getStringResource(R.string.notification_action_open))
)
