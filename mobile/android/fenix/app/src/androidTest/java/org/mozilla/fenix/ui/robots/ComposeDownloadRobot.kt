/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.filter
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onChildren
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.library.downloads.DownloadsListTestTag

/**
 * Implementation of Robot Pattern for download UI handling.
 */

class ComposeDownloadRobot(
    private val testRule: HomeActivityComposeTestRule,
) : DownloadRobot() {

    @OptIn(ExperimentalTestApi::class)
    fun verifyDownloadedFileExistsInDownloadsList(fileName: String) {
        infoLog("verifyDownloadedFileName: Trying to verify that the downloaded file: $fileName is displayed")
        testRule.waitUntilAtLeastOneExists(
            hasTestTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM}.$fileName"),
        )
        testRule.onNodeWithTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM}.$fileName")
            .assertIsDisplayed()
        infoLog("verifyDownloadedFileName: Trying to verify that the downloaded file: $fileName is displayed")
    }

    fun verifyEmptyDownloadsList() {
        infoLog("verifyEmptyDownloadsList: Trying to verify that the \"No downloaded files\" list message is displayed")
        testRule.onNodeWithText(text = testRule.activity.getString(R.string.download_empty_message_1))
            .assertIsDisplayed()
        infoLog("verifyEmptyDownloadsList: Verified that the \"No downloaded files\" list message is displayed")
    }

    fun deleteDownloadedItem(fileName: String) {
        infoLog("deleteDownloadedItem: Trying to click the trash bin icon to delete downloaded file: $fileName")
        testRule.onNodeWithTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM}.$fileName")
            .onChildren()
            .filter(hasContentDescription(testRule.activity.getString(R.string.download_delete_item_1)))
            .onFirst()
            .performClick()
        infoLog("deleteDownloadedItem: Clicked the trash bin icon to delete downloaded file: $fileName")
    }

    fun clickDownloadedItem(fileName: String) {
        infoLog("clickDownloadedItem: Trying to click downloaded file: $fileName")
        testRule.onNodeWithTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM}.$fileName")
            .performClick()
        infoLog("clickDownloadedItem: Clicked downloaded file: $fileName")
    }

    fun longClickDownloadedItem(title: String) {
        infoLog("longClickDownloadedItem: Trying to long click downloaded file: $title")
        testRule.onNodeWithTag("${DownloadsListTestTag.DOWNLOADS_LIST_ITEM}.$title")
            .performTouchInput {
                longClick()
            }
        infoLog("longClickDownloadedItem: Long clicked downloaded file: $title")
    }

    private fun infoLog(message: String) {
        Log.i(TAG, message)
    }
}
