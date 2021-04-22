/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.content.DownloadState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class DownloadFragmentStoreTest {
    private val downloadItem = DownloadItem(
        id = "0",
        url = "url",
        fileName = "title",
        filePath = "url",
        size = "77",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED
    )
    private val newDownloadItem = DownloadItem(
        id = "1",
        url = "url",
        fileName = "title",
        filePath = "url",
        size = "77",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED
    )

    @Test
    fun exitEditMode() = runBlocking {
        val initialState = oneItemEditState()
        val store = DownloadFragmentStore(initialState)

        store.dispatch(DownloadFragmentAction.ExitEditMode).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, DownloadFragmentState.Mode.Normal)
    }

    @Test
    fun itemAddedForRemoval() = runBlocking {
        val initialState = emptyDefaultState()
        val store = DownloadFragmentStore(initialState)

        store.dispatch(DownloadFragmentAction.AddItemForRemoval(newDownloadItem)).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            DownloadFragmentState.Mode.Editing(setOf(newDownloadItem))
        )
    }

    @Test
    fun removeItemForRemoval() = runBlocking {
        val initialState = twoItemEditState()
        val store = DownloadFragmentStore(initialState)

        store.dispatch(DownloadFragmentAction.RemoveItemForRemoval(newDownloadItem)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, DownloadFragmentState.Mode.Editing(setOf(downloadItem)))
    }

    private fun emptyDefaultState(): DownloadFragmentState = DownloadFragmentState(
        items = listOf(),
        mode = DownloadFragmentState.Mode.Normal,
        pendingDeletionIds = emptySet(),
        isDeletingItems = false
    )

    private fun oneItemEditState(): DownloadFragmentState = DownloadFragmentState(
        items = listOf(),
        mode = DownloadFragmentState.Mode.Editing(setOf(downloadItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false
    )

    private fun twoItemEditState(): DownloadFragmentState = DownloadFragmentState(
        items = listOf(),
        mode = DownloadFragmentState.Mode.Editing(setOf(downloadItem, newDownloadItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false
    )
}
