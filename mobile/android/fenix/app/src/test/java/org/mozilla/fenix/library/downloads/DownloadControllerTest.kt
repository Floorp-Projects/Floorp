/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.browser.state.state.content.DownloadState
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DownloadControllerTest {
    private val downloadItem = DownloadItem(
        id = "0",
        url = "url",
        fileName = "title",
        filePath = "url",
        size = "77",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED
    )
    private val scope = TestCoroutineScope()
    private val store: DownloadFragmentStore = mockk(relaxed = true)
    private val state: DownloadFragmentState = mockk(relaxed = true)
    private val openToFileManager: (DownloadItem, BrowsingMode?) -> Unit = mockk(relaxed = true)
    private val displayDeleteAll: () -> Unit = mockk(relaxed = true)
    private val invalidateOptionsMenu: () -> Unit = mockk(relaxed = true)
    private val deleteDownloadItems: (Set<DownloadItem>) -> Unit = mockk(relaxed = true)
    private val controller = DefaultDownloadController(
        store,
        openToFileManager,
        displayDeleteAll,
        invalidateOptionsMenu,
        deleteDownloadItems
    )

    @Before
    fun setUp() {
        every { store.state } returns state
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
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

        verify {
            invalidateOptionsMenu.invoke()
        }
    }

    @Test
    fun onDeleteAll() {
        controller.handleDeleteAll()

        verify {
            displayDeleteAll.invoke()
        }
    }

    @Test
    fun onDeleteSome() {
        val itemsToDelete = setOf(downloadItem)

        controller.handleDeleteSome(itemsToDelete)

        verify {
            deleteDownloadItems(itemsToDelete)
        }
    }
}
