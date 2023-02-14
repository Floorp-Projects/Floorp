/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import org.mozilla.fenix.browser.browsingmode.BrowsingMode

interface DownloadController {
    fun handleOpen(item: DownloadItem, mode: BrowsingMode? = null)
    fun handleSelect(item: DownloadItem)
    fun handleDeselect(item: DownloadItem)
    fun handleBackPressed(): Boolean
    fun handleModeSwitched()
    fun handleDeleteSome(items: Set<DownloadItem>)
}

class DefaultDownloadController(
    private val store: DownloadFragmentStore,
    private val openToFileManager: (item: DownloadItem, mode: BrowsingMode?) -> Unit,
    private val invalidateOptionsMenu: () -> Unit,
    private val deleteDownloadItems: (Set<DownloadItem>) -> Unit,
) : DownloadController {
    override fun handleOpen(item: DownloadItem, mode: BrowsingMode?) {
        openToFileManager(item, mode)
    }

    override fun handleSelect(item: DownloadItem) {
        store.dispatch(DownloadFragmentAction.AddItemForRemoval(item))
    }

    override fun handleDeselect(item: DownloadItem) {
        store.dispatch(DownloadFragmentAction.RemoveItemForRemoval(item))
    }

    override fun handleBackPressed(): Boolean {
        return if (store.state.mode is DownloadFragmentState.Mode.Editing) {
            store.dispatch(DownloadFragmentAction.ExitEditMode)
            true
        } else {
            false
        }
    }

    override fun handleModeSwitched() {
        invalidateOptionsMenu.invoke()
    }

    override fun handleDeleteSome(items: Set<DownloadItem>) {
        deleteDownloadItems.invoke(items)
    }
}
