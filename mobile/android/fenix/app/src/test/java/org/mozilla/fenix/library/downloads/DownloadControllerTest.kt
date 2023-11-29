/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.content.DownloadState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

class DownloadControllerTest {
    private val downloadItem = DownloadItem(
        id = "0",
        url = "url",
        fileName = "title",
        filePath = "url",
        size = "77",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED,
    )
    private val store: DownloadFragmentStore = mockk(relaxed = true)
    private val state: DownloadFragmentState = mockk(relaxed = true)

    private val openToFileManager: (DownloadItem) -> Unit = { item ->
        openToFileManagerCapturedItem = item
    }
    private var openToFileManagerCapturedItem: DownloadItem? = null

    private val invalidateOptionsMenu: () -> Unit = { wasInvalidateOptionsMenuCalled = true }
    private var wasInvalidateOptionsMenuCalled = false

    private val deleteDownloadItems: (Set<DownloadItem>) -> Unit = { deleteDownloadItemsCapturedItems = it }
    private var deleteDownloadItemsCapturedItems = emptySet<DownloadItem>()

    private val controller = DefaultDownloadController(
        store,
        openToFileManager,
        invalidateOptionsMenu,
        deleteDownloadItems,
    )

    @Before
    fun setUp() {
        every { store.state } returns state
    }

    @Test
    fun onPressDownloadItemInNormalMode() {
        controller.handleOpen(downloadItem)

        assertEquals(downloadItem, openToFileManagerCapturedItem)
    }

    @Test
    fun onOpenItem() {
        controller.handleOpen(downloadItem)

        assertEquals(downloadItem, openToFileManagerCapturedItem)
    }

    @Test
    fun onBackPressedInNormalMode() {
        every { state.mode } returns DownloadFragmentState.Mode.Normal

        assertFalse(controller.handleBackPressed())
    }

    @Test
    fun onPressDownloadItemInEditMode() {
        every { state.mode } returns DownloadFragmentState.Mode.Editing(setOf())

        controller.handleSelect(downloadItem)

        verify {
            store.dispatch(DownloadFragmentAction.AddItemForRemoval(downloadItem))
        }
    }

    @Test
    fun onPressSelectedDownloadItemInEditMode() {
        every { state.mode } returns DownloadFragmentState.Mode.Editing(setOf(downloadItem))

        controller.handleDeselect(downloadItem)

        verify {
            store.dispatch(DownloadFragmentAction.RemoveItemForRemoval(downloadItem))
        }
    }

    @Test
    fun onModeSwitched() {
        controller.handleModeSwitched()

        assertTrue(wasInvalidateOptionsMenuCalled)
    }

    @Test
    fun onDeleteSome() {
        val itemsToDelete = setOf(downloadItem)

        controller.handleDeleteSome(itemsToDelete)

        assertEquals(itemsToDelete, deleteDownloadItemsCapturedItems)
    }
}
