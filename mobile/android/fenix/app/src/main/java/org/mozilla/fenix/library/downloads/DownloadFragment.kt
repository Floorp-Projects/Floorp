/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import android.content.DialogInterface
import android.os.Bundle
import android.text.SpannableString
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import kotlinx.android.synthetic.main.fragment_downloads.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.state.BrowserState
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.showSnackBar
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.filterNotExistsOnDisk
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.utils.allowUndo

@SuppressWarnings("TooManyFunctions", "LargeClass")
class DownloadFragment : LibraryPageFragment<DownloadItem>(), UserInteractionHandler {
    private lateinit var downloadStore: DownloadFragmentStore
    private lateinit var downloadView: DownloadView
    private lateinit var downloadInteractor: DownloadInteractor
    private var undoScope: CoroutineScope? = null
    private var pendingDownloadDeletionJob: (suspend () -> Unit)? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_downloads, container, false)

        val items = provideDownloads(requireComponents.core.store.state)

        downloadStore = StoreProvider.get(this) {
            DownloadFragmentStore(
                DownloadFragmentState(
                    items = items,
                    mode = DownloadFragmentState.Mode.Normal,
                    pendingDeletionIds = emptySet(),
                    isDeletingItems = false
                )
            )
        }
        val downloadController: DownloadController = DefaultDownloadController(
            downloadStore,
            ::openItem,
            ::displayDeleteAll,
            ::invalidateOptionsMenu,
            ::deleteDownloadItems
        )
        downloadInteractor = DownloadInteractor(
            downloadController
        )
        downloadView = DownloadView(view.downloadsLayout, downloadInteractor)

        return view
    }

    @VisibleForTesting
    internal fun provideDownloads(state: BrowserState): List<DownloadItem> {
        return state.downloads.values
            .sortedByDescending { it.createdTime } // sort from newest to oldest
            .map {
                DownloadItem(
                    it.id,
                    it.fileName,
                    it.filePath,
                    it.contentLength?.toString() ?: "0",
                    it.contentType,
                    it.status
                )
            }.filter {
                it.status == DownloadState.Status.COMPLETED
            }.filterNotExistsOnDisk()
    }

    override val selectedItems get() = downloadStore.state.mode.selectedItems

    private fun invalidateOptionsMenu() {
        activity?.invalidateOptionsMenu()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    private fun displayDeleteAll() {
        activity?.let { activity ->
            AlertDialog.Builder(activity).apply {
                setMessage(R.string.download_delete_all_dialog)
                setNegativeButton(R.string.delete_browsing_data_prompt_cancel) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
                setPositiveButton(R.string.delete_browsing_data_prompt_allow) { dialog: DialogInterface, _ ->
                    // Use fragment's lifecycle; the view may be gone by the time dialog is interacted with.
                    lifecycleScope.launch(IO) {
                        context.let {
                            it.components.useCases.downloadUseCases.removeAllDownloads()
                        }
                        updatePendingDownloadToDelete(downloadStore.state.items.toSet())
                        launch(Dispatchers.Main) {
                            showSnackBar(
                                requireView(),
                                getString(R.string.download_delete_multiple_items_snackbar)
                            )
                        }
                    }
                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    private fun deleteDownloadItems(items: Set<DownloadItem>) {
        updatePendingDownloadToDelete(items)
        undoScope = CoroutineScope(IO)
        undoScope?.allowUndo(
            requireView(),
            getMultiSelectSnackBarMessage(items),
            getString(R.string.bookmark_undo_deletion),
            {
                undoPendingDeletion(items)
            },
            getDeleteDownloadItemsOperation(items)
        )
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(downloadStore) {
            downloadView.update(it)
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.library_downloads))
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        val menuRes = when (downloadStore.state.mode) {
            is DownloadFragmentState.Mode.Normal -> R.menu.library_menu
            is DownloadFragmentState.Mode.Editing -> R.menu.download_select_multi
        }
        inflater.inflate(menuRes, menu)

        menu.findItem(R.id.delete_downloads_multi_select)?.title =
            SpannableString(getString(R.string.bookmark_menu_delete_button)).apply {
                setTextColor(requireContext(), R.attr.destructive)
            }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.close_history -> {
            close()
            true
        }

        R.id.delete_downloads_multi_select -> {
            deleteDownloadItems(downloadStore.state.mode.selectedItems)
            downloadStore.dispatch(DownloadFragmentAction.ExitEditMode)
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    private fun getMultiSelectSnackBarMessage(downloadItems: Set<DownloadItem>): String {
        return if (downloadItems.size > 1) {
            getString(R.string.download_delete_multiple_items_snackbar)
        } else {
            String.format(
                requireContext().getString(
                    R.string.history_delete_single_item_snackbar
                ), downloadItems.first().fileName
            )
        }
    }

    override fun onPause() {
        invokePendingDeletion()
        super.onPause()
    }

    override fun onBackPressed(): Boolean {
        invokePendingDeletion()
        return downloadView.onBackPressed()
    }

    private fun openItem(item: DownloadItem, mode: BrowsingMode? = null) {

        mode?.let { (activity as HomeActivity).browsingModeManager.mode = it }
        context?.let {
            AbstractFetchDownloadService.openFile(
                context = it,
                contentType = item.contentType,
                filePath = item.filePath
            )
        }
    }

    private fun getDeleteDownloadItemsOperation(items: Set<DownloadItem>): (suspend () -> Unit) {
        return {
            CoroutineScope(IO).launch {
                downloadStore.dispatch(DownloadFragmentAction.EnterDeletionMode)
                context?.let {
                    for (item in items) {
                        it.components.useCases.downloadUseCases.removeDownload(item.id)
                    }
                }
                downloadStore.dispatch(DownloadFragmentAction.ExitDeletionMode)
                pendingDownloadDeletionJob = null
            }
        }
    }

    private fun updatePendingDownloadToDelete(items: Set<DownloadItem>) {
        pendingDownloadDeletionJob = getDeleteDownloadItemsOperation(items)
        val ids = items.map { item -> item.id }.toSet()
        downloadStore.dispatch(DownloadFragmentAction.AddPendingDeletionSet(ids))
    }

    private fun undoPendingDeletion(items: Set<DownloadItem>) {
        pendingDownloadDeletionJob = null
        val ids = items.map { item -> item.id }.toSet()
        downloadStore.dispatch(DownloadFragmentAction.UndoPendingDeletionSet(ids))
    }

    private fun invokePendingDeletion() {
        pendingDownloadDeletionJob?.let {
            viewLifecycleOwner.lifecycleScope.launch {
                it.invoke()
            }.invokeOnCompletion {
                pendingDownloadDeletionJob = null
            }
        }
    }
}
