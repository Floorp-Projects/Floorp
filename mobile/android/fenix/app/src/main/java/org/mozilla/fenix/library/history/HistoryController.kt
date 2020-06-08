/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.fenix.library.history

import android.content.ClipData
import android.content.ClipboardManager
import android.content.res.Resources
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.prompt.ShareData
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar

interface HistoryController {
    fun handleOpen(item: HistoryItem, mode: BrowsingMode? = null)
    fun handleSelect(item: HistoryItem)
    fun handleDeselect(item: HistoryItem)
    fun handleBackPressed(): Boolean
    fun handleModeSwitched()
    fun handleDeleteAll()
    fun handleDeleteSome(items: Set<HistoryItem>)
    fun handleCopyUrl(item: HistoryItem)
    fun handleShare(item: HistoryItem)
    fun handleRequestSync()
}

class DefaultHistoryController(
    private val store: HistoryFragmentStore,
    private val navController: NavController,
    private val resources: Resources,
    private val snackbar: FenixSnackbar,
    private val clipboardManager: ClipboardManager,
    private val scope: CoroutineScope,
    private val openToBrowser: (item: HistoryItem, mode: BrowsingMode?) -> Unit,
    private val displayDeleteAll: () -> Unit,
    private val invalidateOptionsMenu: () -> Unit,
    private val deleteHistoryItems: (Set<HistoryItem>) -> Unit,
    private val syncHistory: suspend () -> Unit
) : HistoryController {
    override fun handleOpen(item: HistoryItem, mode: BrowsingMode?) {
        openToBrowser(item, mode)
    }

    override fun handleSelect(item: HistoryItem) {
        if (store.state.mode === HistoryFragmentState.Mode.Syncing) {
            return
        }
        store.dispatch(HistoryFragmentAction.AddItemForRemoval(item))
    }

    override fun handleDeselect(item: HistoryItem) {
        store.dispatch(HistoryFragmentAction.RemoveItemForRemoval(item))
    }

    override fun handleBackPressed(): Boolean {
        return if (store.state.mode is HistoryFragmentState.Mode.Editing) {
            store.dispatch(HistoryFragmentAction.ExitEditMode)
            true
        } else {
            false
        }
    }

    override fun handleModeSwitched() {
        invalidateOptionsMenu.invoke()
    }

    override fun handleDeleteAll() {
        displayDeleteAll.invoke()
    }

    override fun handleDeleteSome(items: Set<HistoryItem>) {
        deleteHistoryItems.invoke(items)
    }

    override fun handleCopyUrl(item: HistoryItem) {
        val urlClipData = ClipData.newPlainText(item.url, item.url)
        clipboardManager.primaryClip = urlClipData
        with(snackbar) {
            setText(resources.getString(R.string.url_copied))
            show()
        }
    }

    override fun handleShare(item: HistoryItem) {
        navController.navigate(
            HistoryFragmentDirections.actionGlobalShareFragment(
                data = arrayOf(ShareData(url = item.url, title = item.title))
            )
        )
    }

    override fun handleRequestSync() {
        scope.launch {
            store.dispatch(HistoryFragmentAction.StartSync)
            syncHistory.invoke()
            store.dispatch(HistoryFragmentAction.FinishSync)
        }
    }
}
