/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import java.io.File

class DownloadFragmentDataMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `WHEN downloads store is initialised THEN downloads state is updated to be sorted by created time`() =
        runTest {
            val downloads = mapOf(
                "1" to DownloadState(
                    id = "1",
                    createdTime = 1,
                    url = "url",
                    fileName = "1.pdf",
                    status = DownloadState.Status.COMPLETED,
                    destinationDirectory = "",
                    directoryPath = "downloads",
                    contentType = "application/pdf",
                ),
                "2" to DownloadState(
                    id = "2",
                    createdTime = 2,
                    url = "url",
                    fileName = "2.pdf",
                    status = DownloadState.Status.FAILED,
                    destinationDirectory = "",
                    directoryPath = "",
                ),
                "3" to DownloadState(
                    id = "3",
                    createdTime = 3,
                    url = "url",
                    fileName = "3.pdf",
                    status = DownloadState.Status.COMPLETED,
                    destinationDirectory = "",
                    directoryPath = "downloads",
                    contentType = "text/plain",
                ),
            )
            val browserStore = BrowserStore(initialState = BrowserState(downloads = downloads))

            File("downloads").mkdirs()
            val file1 = File("downloads/1.pdf")
            val file3 = File("downloads/3.pdf")

            // Create files
            file1.createNewFile()
            file3.createNewFile()

            val downloadsStore = DownloadFragmentStore(
                initialState = DownloadFragmentState.INITIAL,
                middleware = listOf(
                    DownloadFragmentDataMiddleware(
                        browserStore = browserStore,
                        scope = scope,
                        ioDispatcher = dispatcher,
                    ),
                ),
            )
            downloadsStore.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            downloadsStore.waitUntilIdle()

            val expectedList = listOf(
                DownloadItem(
                    id = "3",
                    url = "url",
                    fileName = "3.pdf",
                    filePath = "downloads/3.pdf",
                    formattedSize = "0",
                    contentType = "text/plain",
                    status = DownloadState.Status.COMPLETED,
                ),
                DownloadItem(
                    id = "1",
                    url = "url",
                    fileName = "1.pdf",
                    filePath = "downloads/1.pdf",
                    formattedSize = "0",
                    contentType = "application/pdf",
                    status = DownloadState.Status.COMPLETED,
                ),
            )

            assertEquals(expectedList, downloadsStore.state.itemsToDisplay)

            // Cleanup files
            file1.delete()
            file3.delete()
        }

    @Test
    fun `WHEN two download states point to the same existing file THEN only one download item is displayed`() =
        runTest {
            val downloads = mapOf(
                "1" to DownloadState(
                    id = "1",
                    createdTime = 1,
                    url = "url",
                    fileName = "1.pdf",
                    status = DownloadState.Status.COMPLETED,
                    contentLength = 100,
                    destinationDirectory = "",
                    directoryPath = "downloads",
                    contentType = "application/pdf",
                ),
                "2" to DownloadState(
                    id = "2",
                    createdTime = 2,
                    url = "url",
                    fileName = "1.pdf",
                    status = DownloadState.Status.COMPLETED,
                    destinationDirectory = "",
                    contentLength = 100,
                    directoryPath = "downloads",
                    contentType = "application/pdf",
                ),
            )
            val browserStore = BrowserStore(initialState = BrowserState(downloads = downloads))

            File("downloads").mkdirs()
            val file1 = File("downloads/1.pdf")
            file1.createNewFile()

            val downloadsStore = DownloadFragmentStore(
                initialState = DownloadFragmentState.INITIAL,
                middleware = listOf(
                    DownloadFragmentDataMiddleware(
                        browserStore = browserStore,
                        scope = scope,
                        ioDispatcher = dispatcher,
                    ),
                ),
            )
            downloadsStore.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            downloadsStore.waitUntilIdle()

            val expectedList = listOf(
                DownloadItem(
                    id = "1",
                    url = "url",
                    fileName = "1.pdf",
                    filePath = "downloads/1.pdf",
                    formattedSize = "0.10 KB",
                    contentType = "application/pdf",
                    status = DownloadState.Status.COMPLETED,
                ),
            )

            assertEquals(expectedList, downloadsStore.state.itemsToDisplay)

            file1.delete()
        }
}
