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
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
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
            scrollToEnd()
            Log.i(TAG, "verifySystemNotificationExists: Scrolling to the end of the notification tray")
            Log.i(TAG, "verifySystemNotificationExists: Looking for $notificationMessage notification")
            notificationFound = mDevice.findObject(notification).waitForExists(waitingTime)
        }

        assertItemContainingTextExists(itemWithText(notificationMessage))
    }

    fun clearNotifications() {
        if (clearButton.exists()) {
            Log.i(TAG, "clearNotifications: Verified that clear notifications button exists")
            clearButton.click()
            Log.i(TAG, "clearNotifications: Clicked clear notifications button")
        } else {
            scrollToEnd()
            Log.i(TAG, "clearNotifications: Scrolled to end of notifications tray")
            if (clearButton.exists()) {
                Log.i(TAG, "clearNotifications: Verified that clear notifications button exists")
                clearButton.click()
                Log.i(TAG, "clearNotifications: Clicked clear notifications button")
            } else if (notificationTray().exists()) {
                mDevice.pressBack()
                Log.i(TAG, "clearNotifications: Dismiss notifications tray by clicking device back button")
            }
        }
    }

    fun cancelAllShownNotifications() {
        cancelAll()
        Log.i(TAG, "cancelAllShownNotifications: Canceled all system notifications")
    }

    fun verifySystemNotificationDoesNotExist(notificationMessage: String) {
        Log.i(TAG, "verifySystemNotificationDoesNotExist: Waiting for $notificationMessage notification to be gone")
        mDevice.findObject(UiSelector().textContains(notificationMessage)).waitUntilGone(waitingTime)
        assertItemContainingTextExists(itemContainingText(notificationMessage), exists = false)
        Log.i(TAG, "verifySystemNotificationDoesNotExist: Verified that $notificationMessage notification does not exist")
    }

    fun verifyPrivateTabsNotification() {
        verifySystemNotificationExists("$appName (Private)")
        verifySystemNotificationExists("Close private tabs")
    }

    fun clickMediaNotificationControlButton(action: String) {
        mediaSystemNotificationButton(action).waitForExists(waitingTime)
        mediaSystemNotificationButton(action).click()
    }

    fun clickDownloadNotificationControlButton(action: String) {
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "clickPageObject: For loop i = $i")
            try {
                assertItemWithResIdAndTextExists(downloadSystemNotificationButton(action))
                downloadSystemNotificationButton(action).clickAndWaitForNewWindow(waitingTimeShort)
                Log.i(TAG, "clickDownloadNotificationControlButton: Clicked app notification $action button and waits for a new window for $waitingTimeShort ms")
                assertItemWithResIdAndTextExists(
                    downloadSystemNotificationButton(action),
                    exists = false,
                )

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "clickDownloadNotificationControlButton: Catch block")
                if (i == RETRY_COUNT) {
                    throw e
                }
                mDevice.waitForWindowUpdate(packageName, waitingTimeShort)
                Log.i(TAG, "clickDownloadNotificationControlButton: Waited $waitingTimeShort ms for window update")
            }
        }
    }

    fun verifyMediaSystemNotificationButtonState(action: String) =
        assertItemWithResIdAndDescriptionExists(mediaSystemNotificationButton(action))

    fun expandNotificationMessage() {
        while (!notificationHeader.exists()) {
            scrollToEnd()
            Log.i(TAG, "expandNotificationMessage: Scrolled to end of notification tray")
        }

        if (notificationHeader.exists()) {
            // expand the notification
            notificationHeader.click()
            Log.i(TAG, "expandNotificationMessage: Clicked the app notification")

            // double check if notification actions are viewable by checking for action existence; otherwise scroll again
            while (!mDevice.findObject(UiSelector().resourceId("android:id/action0")).exists() &&
                !mDevice.findObject(UiSelector().resourceId("android:id/actions_container")).exists()
            ) {
                Log.i(TAG, "expandNotificationMessage: App notification action buttons do not exist")
                scrollToEnd()
                Log.i(TAG, "expandNotificationMessage: Scrolled to end of notification tray")
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
            Log.i(TAG, "swipeDownloadNotification: For loop i = $i")
            try {
                Log.i(TAG, "swipeDownloadNotification: Try block")
                var retries = 0
                while (itemContainingText(appName).exists() && retries++ < 3) {
                    Log.i(TAG, "swipeDownloadNotification: While loop retries = $retries")
                    // Swipe left the download system notification
                    if (direction == "Left") {
                        itemContainingText(appName)
                            .also {
                                it.waitForExists(waitingTime)
                                it.swipeLeft(3)
                            }
                        Log.i(TAG, "swipeDownloadNotification: Swiped left download notification")
                    } else {
                        // Swipe right the download system notification
                        itemContainingText(appName)
                            .also {
                                it.waitForExists(waitingTime)
                                it.swipeRight(3)
                            }
                        Log.i(TAG, "swipeDownloadNotification: Swiped right download notification")
                    }
                }
                // Not all download related system notifications can be dismissed
                if (shouldDismissNotification) {
                    assertItemContainingTextExists(itemContainingText(appName), exists = false)
                } else {
                    assertItemContainingTextExists(itemContainingText(appName))
                    Log.i(TAG, "swipeDownloadNotification: Verified that $appName notification exist")
                }

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "swipeDownloadNotification: Catch block")
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    notificationShade {
                    }.closeNotificationTray {
                    }.openNotificationShade {
                        // The download complete system notification can't be expanded
                        if (canExpandNotification) {
                            // In all cases the download system notification title will be the app name
                            verifySystemNotificationExists(appName)
                            Log.i(TAG, "swipeDownloadNotification: Verified that $appName notification exist")
                            expandNotificationMessage()
                        } else {
                            // Using the download completed system notification summary to bring in to view an properly verify it
                            verifySystemNotificationExists("Download completed")
                        }
                    }
                }
            }
        }
    }

    fun clickNotification(notificationMessage: String) {
        Log.i(TAG, "clickNotification: Looking for $notificationMessage notification")
        mDevice.findObject(UiSelector().text(notificationMessage)).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().text(notificationMessage)).clickAndWaitForNewWindow(waitingTimeShort)
        Log.i(TAG, "clickNotification: Clicked $notificationMessage notification and waiting for $waitingTimeShort ms for a new window")
    }

    class Transition {

        fun clickClosePrivateTabsNotification(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            try {
                assertItemContainingTextExists(closePrivateTabsNotification())
            } catch (e: AssertionError) {
                notificationTray().flingToEnd(1)
            }

            closePrivateTabsNotification().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun closeNotificationTray(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressBack()
            Log.i(TAG, "closeNotificationTray: Closed notification tray using device back button")

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
    notificationTray().scrollToEnd(1)
}

private val clearButton = mDevice.findObject(UiSelector().resourceId("com.android.systemui:id/dismiss_text"))

private fun cancelAll() {
    val notificationManager: NotificationManager =
        TestHelper.appContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
    notificationManager.cancelAll()
}
