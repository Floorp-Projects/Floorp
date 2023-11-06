/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertExternalAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.getPermissionAllowID
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_APPS_PHOTOS
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for download UI handling.
 */

class DownloadRobot {

    fun verifyDownloadPrompt(fileName: String) {
        var currentTries = 0
        while (currentTries++ < 3) {
            Log.i(TAG, "verifyDownloadPrompt: While loop currentTries = $currentTries")
            try {
                Log.i(TAG, "verifyDownloadPrompt: Try block")
                assertTrue(
                    "Download prompt button not visible",
                    mDevice.findObject(UiSelector().resourceId("$packageName:id/download_button"))
                        .waitForExists(waitingTimeLong),
                )
                Log.i(TAG, "verifyDownloadPrompt: Verified that the \"DOWNLOAD\" prompt button exists")
                assertTrue(
                    "$fileName title doesn't match",
                    mDevice.findObject(UiSelector().text(fileName))
                        .waitForExists(waitingTimeLong),
                )
                Log.i(TAG, "verifyDownloadPrompt: Verified that the download prompt for $fileName exists")

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "verifyDownloadPrompt: Catch block")
                Log.e("DOWNLOAD_ROBOT", "Failed to find locator: ${e.localizedMessage}")

                browserScreen {
                }.clickDownloadLink(fileName) {
                }
            }
        }
    }

    fun verifyDownloadCompleteNotificationPopup() {
        assertTrue(
            "Download notification Open button not found",
            mDevice.findObject(UiSelector().text("Open"))
                .waitForExists(waitingTime),
        )
        assertTrue(
            "Download completed notification text doesn't match",
            mDevice.findObject(UiSelector().textContains("Download completed"))
                .waitForExists(waitingTime),
        )
        assertTrue(
            "Downloaded file name not visible",
            mDevice.findObject(UiSelector().resourceId("$packageName:id/download_dialog_filename"))
                .waitForExists(waitingTime),
        )
    }

    fun verifyDownloadFailedPrompt(fileName: String) {
        assertTrue(
            itemWithResId("$packageName:id/download_dialog_icon")
                .waitForExists(waitingTime),
        )
        assertTrue(
            "Download dialog title not displayed",
            itemWithResIdAndText(
                "$packageName:id/download_dialog_title",
                "Download failed",
            ).waitForExists(waitingTime),
        )
        assertTrue(
            "Download file name not displayed",
            itemWithResIdContainingText(
                "$packageName:id/download_dialog_filename",
                fileName,
            ).waitForExists(waitingTime),
        )
        assertTrue(
            "Try again button not displayed",
            itemWithResIdAndText(
                "$packageName:id/download_dialog_action_button",
                "Try Again",
            ).waitForExists(waitingTime),
        )
    }

    fun clickTryAgainButton() {
        itemWithResIdAndText(
            "$packageName:id/download_dialog_action_button",
            "Try Again",
        ).click()
    }

    fun verifyPhotosAppOpens() = assertExternalAppOpens(GOOGLE_APPS_PHOTOS)

    fun verifyDownloadedFileName(fileName: String) {
        assertTrue(
            "$fileName not found in Downloads list",
            mDevice.findObject(UiSelector().text(fileName))
                .waitForExists(waitingTime),
        )
    }

    fun verifyDownloadedFileIcon() = assertDownloadedFileIcon()

    fun verifyEmptyDownloadsList() {
        mDevice.findObject(UiSelector().resourceId("$packageName:id/download_empty_view"))
            .waitForExists(waitingTime)
        onView(withText("No downloaded files")).check(matches(isDisplayed()))
    }

    fun waitForDownloadsListToExist() =
        assertTrue(
            "Downloads list either empty or not displayed",
            mDevice.findObject(UiSelector().resourceId("$packageName:id/download_list"))
                .waitForExists(waitingTime),
        )

    fun openDownloadedFile(fileName: String) {
        downloadedFile(fileName)
            .check(matches(isDisplayed()))
            .click()
    }

    fun deleteDownloadedItem(fileName: String) =
        onView(
            allOf(
                withId(R.id.overflow_menu),
                hasSibling(withText(fileName)),
            ),
        ).click()

    fun longClickDownloadedItem(title: String) =
        onView(
            allOf(
                withId(R.id.title),
                withText(title),
            ),
        ).perform(longClick())

    fun selectDownloadedItem(title: String) =
        onView(
            allOf(
                withId(R.id.title),
                withText(title),
            ),
        ).perform(click())

    fun openMultiSelectMoreOptionsMenu() =
        itemWithDescription(getStringResource(R.string.content_description_menu)).click()

    fun clickMultiSelectRemoveButton() =
        itemWithResIdContainingText("$packageName:id/title", "Remove").click()

    fun openPageAndDownloadFile(url: Uri, downloadFile: String) {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(url) {
            waitForPageToLoad()
        }.clickDownloadLink(downloadFile) {
            verifyDownloadPrompt(downloadFile)
        }.clickDownload {
        }
    }

    class Transition {
        fun clickDownload(interact: DownloadRobot.() -> Unit): Transition {
            downloadButton().click()
            Log.i(TAG, "clickDownload: Clicked \"DOWNLOAD\" button from prompt")

            DownloadRobot().interact()
            return Transition()
        }

        fun closeCompletedDownloadPrompt(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            closeCompletedDownloadButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeDownloadPrompt(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            itemWithResId("$packageName:id/download_dialog_close_button").click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickOpen(type: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            openDownloadButton().waitForExists(waitingTime)
            openDownloadButton().click()

            // verify open intent is matched with associated data type
            Intents.intended(
                CoreMatchers.allOf(
                    IntentMatchers.hasAction(Intent.ACTION_VIEW),
                    IntentMatchers.hasType(type),
                ),
            )

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickAllowPermission(interact: DownloadRobot.() -> Unit): Transition {
            mDevice.waitNotNull(
                Until.findObject(By.res(getPermissionAllowID() + ":id/permission_allow_button")),
                waitingTime,
            )

            val allowPermissionButton = mDevice.findObject(By.res(getPermissionAllowID() + ":id/permission_allow_button"))
            allowPermissionButton.click()

            DownloadRobot().interact()
            return Transition()
        }

        fun exitDownloadsManagerToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withContentDescription("Navigate up")).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun downloadRobot(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
    DownloadRobot().interact()
    return DownloadRobot.Transition()
}

private fun closeCompletedDownloadButton() =
    onView(withId(R.id.download_dialog_close_button))

private fun closePromptButton() =
    onView(withId(R.id.close_button))

private fun downloadButton() =
    onView(withId(R.id.download_button))
        .check(matches(isDisplayed()))

private fun openDownloadButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/download_dialog_action_button"))

private fun downloadedFile(fileName: String) = onView(withText(fileName))

private fun assertDownloadedFileIcon() =
    assertTrue(
        "Downloaded file icon not found",
        mDevice.findObject(UiSelector().resourceId("$packageName:id/favicon"))
            .exists(),
    )

private fun goBackButton() = onView(withContentDescription("Navigate up"))
