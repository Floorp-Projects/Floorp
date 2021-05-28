/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Intent
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_APPS_PHOTOS
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for download UI handling.
 */

class DownloadRobot {

    fun verifyDownloadPrompt() = assertDownloadPrompt()

    fun verifyDownloadNotificationPopup() = assertDownloadNotificationPopup()

    fun verifyPhotosAppOpens() = assertPhotosOpens()

    fun verifyDownloadedFileName(fileName: String) {
        mDevice.findObject(UiSelector().text(fileName)).waitForExists(waitingTime)
        downloadedFile(fileName).check(matches(isDisplayed()))
    }

    fun verifyDownloadedFileIcon() = assertDownloadedFileIcon()

    fun verifyEmptyDownloadsList() {
        mDevice.findObject(UiSelector().resourceId("$packageName:id/download_empty_view"))
            .waitForExists(waitingTime)
        onView(withText("No downloaded files")).check(matches(isDisplayed()))
    }

    fun waitForDownloadsListToExist() =
        assertTrue(mDevice.findObject(UiSelector().resourceId("$packageName:id/download_list"))
            .waitForExists(waitingTime))

    class Transition {
        fun clickDownload(interact: DownloadRobot.() -> Unit): Transition {
            clickDownloadButton().click()

            DownloadRobot().interact()
            return Transition()
        }

        fun closePrompt(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            closePromptButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickOpen(type: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            clickOpenButton().click()

            // verify open intent is matched with associated data type
            Intents.intended(
                CoreMatchers.allOf(
                    IntentMatchers.hasAction(Intent.ACTION_VIEW),
                    IntentMatchers.hasType(type)
                )
            )

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickAllowPermission(interact: DownloadRobot.() -> Unit): Transition {
            val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

            mDevice.waitNotNull(
                Until.findObject(By.res(TestHelper.getPermissionAllowID() + ":id/permission_allow_button")),
                TestAssetHelper.waitingTime
            )

            val allowPermissionButton = mDevice.findObject(By.res(TestHelper.getPermissionAllowID() + ":id/permission_allow_button"))
            allowPermissionButton.click()

            DownloadRobot().interact()
            return Transition()
        }

        fun exitDownloadsManagerToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withContentDescription("Navigate up")).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun downloadRobot(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
    DownloadRobot().interact()
    return DownloadRobot.Transition()
}

private fun assertDownloadPrompt() {
    mDevice.waitNotNull(Until.findObjects(By.res("$packageName:id/download_button")))
}

private fun assertDownloadNotificationPopup() {
    val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    mDevice.waitNotNull(Until.findObjects(By.text("Open")), TestAssetHelper.waitingTime)
    onView(withId(R.id.download_dialog_title))
        .check(matches(withText(CoreMatchers.containsString("Download completed"))))
}

private fun closePromptButton() =
    onView(withId(R.id.close_button)).inRoot(isDialog()).check(matches(isDisplayed()))

private fun clickDownloadButton() =
    onView(withText("Download")).inRoot(isDialog()).check(matches(isDisplayed()))

private fun clickOpenButton() =
    onView(withId(R.id.download_dialog_action_button)).check(
        matches(isDisplayed())
    )

private fun assertPhotosOpens() {
    if (isPackageInstalled(GOOGLE_APPS_PHOTOS)) {
        Intents.intended(IntentMatchers.toPackage(GOOGLE_APPS_PHOTOS))
    } else {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(
            Until.findObject(By.text("Could not open file")),
            TestAssetHelper.waitingTime
        )
    }
}

private fun downloadedFile(fileName: String) = onView(withText(fileName))

private fun assertDownloadedFileIcon() = onView(withId(R.id.favicon)).check(matches(isDisplayed()))
