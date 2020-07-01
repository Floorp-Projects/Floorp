/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DownloadControllerTest {
    private val downloadItem = DownloadItem(0, "title", "url", "77", "jpg")
    private val scope: CoroutineScope = TestCoroutineScope()
    private val store: DownloadFragmentStore = mockk(relaxed = true)
    private val state: DownloadFragmentState = mockk(relaxed = true)
    private val openToFileManager: (DownloadItem, BrowsingMode?) -> Unit = mockk(relaxed = true)
    private val invalidateOptionsMenu: () -> Unit = mockk(relaxed = true)
    private val controller = DefaultDownloadController(
        store,
        openToFileManager
    )

    @Before
    fun setUp() {
        every { store.state } returns state
    }

    @Test
    fun onPressDownloadItemInNormalMode() {
        controller.handleOpen(downloadItem)

        verify {
            openToFileManager(downloadItem, null)
        }
    }

    @Test
    fun onOpenItemInNormalMode() {
        controller.handleOpen(downloadItem, BrowsingMode.Normal)

        verify {
            openToFileManager(downloadItem, BrowsingMode.Normal)
        }
    }

    @Test
    fun onBackPressedInNormalMode() {
        every { state.mode } returns DownloadFragmentState.Mode.Normal

        assertFalse(controller.handleBackPressed())
    }
}
