/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.app.NotificationManager
import android.content.Context
import android.util.Log
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import kotlin.AssertionError

class NotificationRobot {

    fun verifySystemNotificationExists(notificationMessage: String) {
        val notification = UiSelector().text(notificationMessage)
        var notificationFound = mDevice.findObject(notification).waitForExists(waitingTime)

        while (!notificationFound) {
            Log.i(TAG, "verifySystemNotificationExists: Waiting for $waitingTime ms for notification: $notification to exist")
            scrollToEnd()
            notificationFound = mDevice.findObject(notification).waitForExists(waitingTime)
            Log.i(TAG, "verifySystemNotificationExists: Waited for $waitingTime ms for notification: $notification to exist")
        }

        assertUIObjectExists(itemWithText(notificationMessage))
    }

    fun clearNotifications() {
        if (clearButton.exists()) {
            Log.i(TAG, "clearNotifications:The clear notifications button exists")
            Log.i(TAG, "clearNotifications: Trying to click the clear notifications button")
            clearButton.click()
            Log.i(TAG, "clearNotifications: Clicked the clear notifications button")
        } else {
            scrollToEnd()
            if (clearButton.exists()) {
                Log.i(TAG, "clearNotifications:The clear notifications button exists")
                Log.i(TAG, "clearNotifications: Trying to click the clear notifications button")
                clearButton.click()
                Log.i(TAG, "clearNotifications: Clicked the clear notifications button")
            } else if (notificationTray().exists()) {
                Log.i(TAG, "clearNotifications: The notifications tray is still displayed")
                Log.i(TAG, "clearNotifications: Trying to click device back button")
                mDevice.pressBack()
                Log.i(TAG, "clearNotifications: Clicked device back button")
            }
        }
    }

    fun cancelAllShownNotifications() {
        Log.i(TAG, "cancelAllShownNotifications: Trying to cancel all system notifications")
        cancelAll()
        Log.i(TAG, "cancelAllShownNotifications: Canceled all system notifications")
    }

    fun verifySystemNotificationDoesNotExist(notificationMessage: String) {
        Log.i(TAG, "verifySystemNotificationDoesNotExist: Waiting for $waitingTime ms for notification: $notificationMessage to be gone")
        mDevice.findObject(UiSelector().textContains(notificationMessage)).waitUntilGone(waitingTime)
        Log.i(TAG, "verifySystemNotificationDoesNotExist: Waited for $waitingTime ms for notification: $notificationMessage to be gone")
        assertUIObjectExists(itemContainingText(notificationMessage), exists = false)
    }

    fun verifyPrivateTabsNotification() {
        verifySystemNotificationExists("$appName (Private)")
        verifySystemNotificationExists("Close private tabs")
    }

    fun clickMediaNotificationControlButton(action: String) {
        Log.i(TAG, "clickMediaNotificationControlButton: Waiting for $waitingTime ms for the system media control button: $action to exist")
        mediaSystemNotificationButton(action).waitForExists(waitingTime)
        Log.i(TAG, "clickMediaNotificationControlButton: Waited for $waitingTime ms for the system media control button: $action to exist")
        Log.i(TAG, "clickMediaNotificationControlButton: Trying to click the system media control button: $action")
        mediaSystemNotificationButton(action).click()
        Log.i(TAG, "clickMediaNotificationControlButton: Clicked the system media control button: $action")
    }

    fun clickDownloadNotificationControlButton(action: String) {
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "clickDownloadNotificationControlButton: Started try #$i")
            try {
                assertUIObjectExists(downloadSystemNotificationButton(action))
                Log.i(TAG, "clickDownloadNotificationControlButton: Trying to click the download system notification: $action button and wait for $waitingTimeShort ms for a new window")
                downloadSystemNotificationButton(action).clickAndWaitForNewWindow(waitingTimeShort)
                Log.i(TAG, "clickDownloadNotificationControlButton: Clicked the download system notification: $action button and waited for $waitingTimeShort ms for a new window")
                assertUIObjectExists(
                    downloadSystemNotificationButton(action),
                    exists = false,
                )

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "clickDownloadNotificationControlButton: AssertionError caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                }
                Log.i(TAG, "clickDownloadNotificationControlButton: Waiting for $waitingTimeShort ms for $packageName window to be updated")
                mDevice.waitForWindowUpdate(packageName, waitingTimeShort)
                Log.i(TAG, "clickDownloadNotificationControlButton: Waited for $waitingTimeShort ms for $packageName window to be updated")
            }
        }
    }

    fun verifyMediaSystemNotificationButtonState(action: String) =
        assertUIObjectExists(mediaSystemNotificationButton(action))

    fun expandNotificationMessage() {
        while (!notificationHeader.exists()) {
            Log.i(TAG, "expandNotificationMessage: Waiting for $appName notification to exist")
            scrollToEnd()
        }

        if (notificationHeader.exists()) {
            Log.i(TAG, "expandNotificationMessage: $appName notification exists")
            // expand the notification
            Log.i(TAG, "expandNotificationMessage: Trying to click $appName notification")
            notificationHeader.click()
            Log.i(TAG, "expandNotificationMessage: Clicked $appName notification")

            // double check if notification actions are viewable by checking for action existence; otherwise scroll again
            while (!mDevice.findObject(UiSelector().resourceId("android:id/action0")).exists() &&
                !mDevice.findObject(UiSelector().resourceId("android:id/actions_container")).exists()
            ) {
                Log.i(TAG, "expandNotificationMessage: App notification action buttons do not exist")
                scrollToEnd()
            }
        }
    }

    // Performs swipe action on download system notifications
    fun swipeDownloadNotification(
        direction: String,
        shouldDismissNotification: Boolean,
        canExpandNotification: Boolean = true,
    ) {
        // In case it fails, retry max 3x the swipe action on download system notifications
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "swipeDownloadNotification: Started try #$i")
            try {
                var retries = 0
                while (itemContainingText(appName).exists() && retries++ < 3) {
                    // Swipe left the download system notification
                    if (direction == "Left") {
                        itemContainingText(appName)
                            .also {
                                Log.i(TAG, "swipeDownloadNotification: Waiting for $waitingTime ms for $appName notification to exist")
                                it.waitForExists(waitingTime)
                                Log.i(TAG, "swipeDownloadNotification: Waited for $waitingTime ms for $appName notification to exist")
                                Log.i(TAG, "swipeDownloadNotification: Trying to perform swipe left action on $appName notification")
                                it.swipeLeft(3)
                                Log.i(TAG, "swipeDownloadNotification: Performed swipe left action on $appName notification")
                            }
                    } else {
                        // Swipe right the download system notification
                        itemContainingText(appName)
                            .also {
                                Log.i(TAG, "swipeDownloadNotification: Waiting for $waitingTime ms for $appName notification to exist")
                                it.waitForExists(waitingTime)
                                Log.i(TAG, "swipeDownloadNotification: Waited for $waitingTime ms for $appName notification to exist")
                                Log.i(TAG, "swipeDownloadNotification: Trying to perform swipe right action on $appName notification")
                                it.swipeRight(3)
                                Log.i(TAG, "swipeDownloadNotification: Performed swipe right action on $appName notification")
                            }
                    }
                }
                // Not all download related system notifications can be dismissed
                if (shouldDismissNotification) {
                    Log.i(TAG, "swipeDownloadNotification: $appName notification can't be dismissed: $shouldDismissNotification")
                    assertUIObjectExists(itemContainingText(appName), exists = false)
                } else {
                    Log.i(TAG, "swipeDownloadNotification: $appName notification can be dismissed: $shouldDismissNotification")
                    assertUIObjectExists(itemContainingText(appName))
                }

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "swipeDownloadNotification: AssertionError caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    notificationShade {
                    }.closeNotificationTray {
                    }.openNotificationShade {
                        // The download complete system notification can't be expanded
                        if (canExpandNotification) {
                            Log.i(TAG, "swipeDownloadNotification: $appName notification can be expanded: $canExpandNotification")
                            // In all cases the download system notification title will be the app name
                            verifySystemNotificationExists(appName)
                            expandNotificationMessage()
                        } else {
                            Log.i(TAG, "swipeDownloadNotification: $appName notification can't be expanded: $canExpandNotification")
                            // Using the download completed system notification summary to bring in to view an properly verify it
                            verifySystemNotificationExists("Download completed")
                        }
                    }
                }
            }
        }
    }

    fun clickNotification(notificationMessage: String) {
        Log.i(TAG, "clickNotification: Waiting for $waitingTime ms for $notificationMessage notification to exist")
        mDevice.findObject(UiSelector().text(notificationMessage)).waitForExists(waitingTime)
        Log.i(TAG, "clickNotification: Waited for $waitingTime ms for $notificationMessage notification to exist")
        Log.i(TAG, "clickNotification: Trying to click the $notificationMessage notification and wait for $waitingTimeShort ms for a new window")
        mDevice.findObject(UiSelector().text(notificationMessage)).clickAndWaitForNewWindow(waitingTimeShort)
        Log.i(TAG, "clickNotification: Clicked the $notificationMessage notification and waited for $waitingTimeShort ms for a new window")
    }

    class Transition {

        fun clickClosePrivateTabsNotification(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            try {
                assertUIObjectExists(closePrivateTabsNotification())
            } catch (e: AssertionError) {
                Log.i(TAG, "clickClosePrivateTabsNotification: Trying to perform fling action to the end of the notification tray")
                notificationTray().flingToEnd(1)
                Log.i(TAG, "clickClosePrivateTabsNotification: Performed fling action to the end of the notification tray")
            }
            Log.i(TAG, "clickClosePrivateTabsNotification: Trying to click the close private tabs notification")
            closePrivateTabsNotification().click()
            Log.i(TAG, "clickClosePrivateTabsNotification: Clicked the close private tabs notification")

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun closeNotificationTray(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "closeNotificationTray: Trying to click device back button")
            mDevice.pressBack()
            Log.i(TAG, "closeNotificationTray: Clicked device back button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun notificationShade(interact: NotificationRobot.() -> Unit): NotificationRobot.Transition {
    NotificationRobot().interact()
    return NotificationRobot.Transition()
}

private fun closePrivateTabsNotification() =
    mDevice.findObject(UiSelector().text("Close private tabs"))

private fun downloadSystemNotificationButton(action: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("android:id/action0")
            .textContains(action),
    )

private fun mediaSystemNotificationButton(action: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("com.android.systemui:id/action0")
            .descriptionContains(action),
    )

private fun notificationTray() = UiScrollable(
    UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller"),
).setAsVerticalList()

private val notificationHeader =
    mDevice.findObject(
        UiSelector()
            .resourceId("android:id/app_name_text")
            .text(appName),
    )

private fun scrollToEnd() {
    Log.i(TAG, "scrollToEnd: Trying to perform scroll to the end of the notification tray action")
    notificationTray().scrollToEnd(1)
    Log.i(TAG, "scrollToEnd: Performed scroll to the end of the notification tray action")
}

private val clearButton = mDevice.findObject(UiSelector().resourceId("com.android.systemui:id/dismiss_text"))

private fun cancelAll() {
    val notificationManager: NotificationManager =
        TestHelper.appContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
    notificationManager.cancelAll()
}
