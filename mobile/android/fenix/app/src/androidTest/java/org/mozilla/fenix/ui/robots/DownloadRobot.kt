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
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertExternalAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.getPermissionAllowID
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_APPS_PHOTOS
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
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
                assertItemWithResIdExists(itemWithResId("$packageName:id/download_button"))
                assertItemContainingTextExists(itemContainingText(fileName))

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
        assertItemContainingTextExists(
            itemContainingText(getStringResource(R.string.mozac_feature_downloads_button_open)),
            itemContainingText(getStringResource(R.string.mozac_feature_downloads_completed_notification_text2)),
        )
        assertItemWithResIdExists(itemWithResId("$packageName:id/download_dialog_filename"))
    }

    fun verifyDownloadFailedPrompt(fileName: String) {
        assertItemWithResIdExists(itemWithResId("$packageName:id/download_dialog_icon"))
        assertItemWithResIdAndTextExists(
            itemWithResIdContainingText(
                "$packageName:id/download_dialog_title",
                getStringResource(R.string.mozac_feature_downloads_failed_notification_text2),
            ),
            itemWithResIdContainingText(
                "$packageName:id/download_dialog_filename",
                fileName,
            ),
            itemWithResIdContainingText(
                "$packageName:id/download_dialog_action_button",
                getStringResource(R.string.mozac_feature_downloads_button_try_again),
            ),
        )
    }

    fun clickTryAgainButton() {
        itemWithResIdAndText(
            "$packageName:id/download_dialog_action_button",
            "Try Again",
        ).click()
        Log.i(TAG, "clickTryAgainButton: Clicked \"TRY AGAIN\" in app prompt button")
    }

    fun verifyPhotosAppOpens() = assertExternalAppOpens(GOOGLE_APPS_PHOTOS)

    fun verifyDownloadedFileName(fileName: String) =
        assertItemContainingTextExists(itemContainingText(fileName))

    fun verifyDownloadedFileIcon() = assertItemWithResIdExists(itemWithResId("$packageName:id/favicon"))

    fun verifyEmptyDownloadsList() {
        Log.i(TAG, "verifyEmptyDownloadsList: Looking for empty download list")
        mDevice.findObject(UiSelector().resourceId("$packageName:id/download_empty_view"))
            .waitForExists(waitingTime)
        onView(withText("No downloaded files")).check(matches(isDisplayed()))
        Log.i(TAG, "verifyEmptyDownloadsList: Verified \"No downloaded files\" list message")
    }

    fun waitForDownloadsListToExist() =
        assertItemWithResIdExists(itemWithResId("$packageName:id/download_list"))

    fun openDownloadedFile(fileName: String) {
        downloadedFile(fileName)
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "openDownloadedFile: Clicked downloaded file: $fileName")
    }

    fun deleteDownloadedItem(fileName: String) {
        onView(
            allOf(
                withId(R.id.overflow_menu),
                hasSibling(withText(fileName)),
            ),
        ).click()
        Log.i(TAG, "deleteDownloadedItem: Deleted downloaded file: $fileName using trash bin icon")
    }

    fun longClickDownloadedItem(title: String) {
        onView(
            allOf(
                withId(R.id.title),
                withText(title),
            ),
        ).perform(longClick())
        Log.i(TAG, "longClickDownloadedItem: Long clicked downloaded file: $title")
    }

    fun selectDownloadedItem(title: String) {
        onView(
            allOf(
                withId(R.id.title),
                withText(title),
            ),
        ).perform(click())
        Log.i(TAG, "selectDownloadedItem: Selected downloaded file: $title")
    }

    fun openMultiSelectMoreOptionsMenu() {
        itemWithDescription(getStringResource(R.string.content_description_menu)).click()
        Log.i(TAG, "openMultiSelectMoreOptionsMenu: Clicked multi-select more options button")
    }

    fun clickMultiSelectRemoveButton() {
        itemWithResIdContainingText("$packageName:id/title", "Remove").click()
        Log.i(TAG, "clickMultiSelectRemoveButton: Clicked multi-select remove button")
    }

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

        fun closeDownloadPrompt(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            itemWithResId("$packageName:id/download_dialog_close_button").click()
            Log.i(TAG, "closeDownloadPrompt: Dismissed download prompt by clicking close prompt button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickOpen(type: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickOpen: Looking for \"OPEN\" download prompt button")
            openDownloadButton().waitForExists(waitingTime)
            openDownloadButton().click()
            Log.i(TAG, "clickOpen: Clicked \"OPEN\" download prompt button")

            // verify open intent is matched with associated data type
            Intents.intended(
                allOf(
                    IntentMatchers.hasAction(Intent.ACTION_VIEW),
                    IntentMatchers.hasType(type),
                ),
            )
            Log.i(TAG, "clickOpen: Verified that open intent is matched with associated data type")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickAllowPermission(interact: DownloadRobot.() -> Unit): Transition {
            Log.i(TAG, "clickAllowPermission: Looking for \"ALLOW\" permission button")
            mDevice.waitNotNull(
                Until.findObject(By.res(getPermissionAllowID() + ":id/permission_allow_button")),
                waitingTime,
            )

            mDevice.findObject(By.res(getPermissionAllowID() + ":id/permission_allow_button")).click()
            Log.i(TAG, "clickAllowPermission: Clicked \"ALLOW\" permission button")

            DownloadRobot().interact()
            return Transition()
        }

        fun exitDownloadsManagerToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withContentDescription("Navigate up")).click()
            Log.i(TAG, "exitDownloadsManagerToBrowser: Exited download manager to browser by clicking the navigate up toolbar button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton().click()
            Log.i(TAG, "exitDownloadsManagerToBrowser: Exited download manager to home screen by clicking the navigate up toolbar button")

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun downloadRobot(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
    DownloadRobot().interact()
    return DownloadRobot.Transition()
}

private fun downloadButton() =
    onView(withId(R.id.download_button))
        .check(matches(isDisplayed()))

private fun openDownloadButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/download_dialog_action_button"))

private fun downloadedFile(fileName: String) = onView(withText(fileName))

private fun goBackButton() = onView(withContentDescription("Navigate up"))
