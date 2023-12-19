/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status.COMPLETED
import mozilla.components.browser.state.state.content.DownloadState.Status.FAILED
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DownloadDialogUtilsTest {

    @Test
    @Suppress("MaxLineLength")
    fun `GIVEN final status WHEN handleOnDownloadFinished try to open the file fails THEN show error message`() {
        val downloadUtils = DownloadDialogUtils
        val currentTab = createTab(id = "1", url = "")
        val download = DownloadState(
            url = "",
            sessionId = currentTab.id,
            destinationDirectory = "/",
            openInApp = true,
        )
        var dialogWasShown = false
        var onCannotOpenFileWasCalled = false

        downloadUtils.openFile = { _, _ -> false }

        downloadUtils.handleOnDownloadFinished(
            context = testContext,
            downloadState = download,
            downloadJobStatus = COMPLETED,
            currentTab = currentTab,
            onFinishedDialogShown = { dialogWasShown = true },
            onCannotOpenFile = {
                onCannotOpenFileWasCalled = true
            },
        )

        assertTrue(onCannotOpenFileWasCalled)
        assertFalse(dialogWasShown)
    }

    @Test
    fun `GIVEN final status and openInApp WHEN calling handleOnDownloadFinished THEN try to open the file`() {
        val downloadUtils = DownloadDialogUtils
        val download = DownloadState(
            url = "",
            sessionId = "1",
            destinationDirectory = "/",
            openInApp = true,
        )
        val currentTab = createTab(id = "1", url = "")
        var dialogWasShown = false
        var onCannotOpenFileWasCalled = false

        downloadUtils.openFile = { _, _ -> true }

        downloadUtils.handleOnDownloadFinished(
            context = testContext,
            downloadState = download,
            downloadJobStatus = COMPLETED,
            currentTab = currentTab,
            onFinishedDialogShown = { dialogWasShown = true },
            onCannotOpenFile = {
                onCannotOpenFileWasCalled = true
            },
        )

        assertFalse(onCannotOpenFileWasCalled)
        assertFalse(dialogWasShown)
    }

    @Test
    fun `GIVEN final status WHEN calling handleOnDownloadFinished THEN show a finished download dialog`() {
        val downloadUtils = DownloadDialogUtils
        val download = DownloadState(
            url = "",
            sessionId = "1",
            destinationDirectory = "/",
        )
        val currentTab = createTab(id = "1", url = "")
        var dialogWasShown = false

        downloadUtils.handleOnDownloadFinished(
            context = testContext,
            downloadState = download,
            downloadJobStatus = COMPLETED,
            currentTab = currentTab,
            onFinishedDialogShown = { dialogWasShown = true },
            onCannotOpenFile = {},
        )

        assertTrue(dialogWasShown)
    }

    @Test
    fun `WHEN status is final and is same tab THEN shouldShowCompletedDownloadDialog will be true`() {
        val currentTab = createTab(id = "1", url = "")

        val download = DownloadState(
            url = "",
            sessionId = "1",
            destinationDirectory = "/",
        )

        val status = DownloadState.Status.values().filter { it == COMPLETED || it == FAILED }

        status.forEach {
            val result = DownloadDialogUtils.shouldShowCompletedDownloadDialog(
                downloadState = download,
                status = it,
                currentTab = currentTab,
            )

            assertTrue(result)
        }
    }

    @Test
    fun `WHEN status is different from FAILED or COMPLETED then shouldShowCompletedDownloadDialog will be false`() {
        val currentTab = createTab(id = "1", url = "")

        val download = DownloadState(
            url = "",
            sessionId = "1",
            destinationDirectory = "/",
        )

        val completedStatus = listOf(COMPLETED, FAILED)
        val status = DownloadState.Status.values().filter { it !in completedStatus }

        status.forEach {
            val result = DownloadDialogUtils.shouldShowCompletedDownloadDialog(
                downloadState = download,
                status = it,
                currentTab = currentTab,
            )

            assertFalse(result)
        }
    }

    @Test
    fun `WHEN the tab is different from the initial one then shouldShowCompletedDownloadDialog will be false`() {
        val currentTab = createTab(id = "1", url = "")

        val download = DownloadState(
            url = "",
            sessionId = "2",
            destinationDirectory = "/",
        )
        val completedStatus = listOf(COMPLETED, FAILED)
        val status = DownloadState.Status.values().filter { it !in completedStatus }
        status.forEach {
            val result =
                DownloadDialogUtils.shouldShowCompletedDownloadDialog(
                    downloadState = download,
                    status = it,
                    currentTab = currentTab,
                )
            assertFalse(result)
        }
    }
}
