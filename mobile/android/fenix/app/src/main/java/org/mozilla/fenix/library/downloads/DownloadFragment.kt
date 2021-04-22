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
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.showSnackBar
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
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
    private lateinit var metrics: MetricController
    private var undoScope: CoroutineScope? = null
    private var pendingDownloadDeletionJob: (suspend () -> Unit)? = null
    private lateinit var downloadsUseCases: DownloadsUseCases

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_downloads, container, false)

        val items = provideDownloads(requireComponents.core.store.state)
        downloadsUseCases = requireContext().components.useCases.downloadUseCases

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

    /**
     * Returns a list of available downloads to be displayed to the user.
     * Downloads must be COMPLETED and existent on disk.
     */
    @VisibleForTesting
    internal fun provideDownloads(state: BrowserState): List<DownloadItem> {
        return state.downloads.values
            .sortedByDescending { it.createdTime } // sort from newest to oldest
            .map {
                DownloadItem(
                    id = it.id,
                    url = it.url,
                    fileName = it.fileName,
                    filePath = it.filePath,
                    size = it.contentLength?.toString() ?: "0",
                    contentType = it.contentType,
                    status = it.status
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

        metrics = requireComponents.analytics.metrics
        metrics.track(Event.DownloadsScreenOpened)
    }

    private fun displayDeleteAll() {
        metrics.track(Event.DownloadsItemDeleted)
        activity?.let { activity ->
            AlertDialog.Builder(activity).apply {
                setMessage(R.string.download_delete_all_dialog)
                setNegativeButton(R.string.delete_browsing_data_prompt_cancel) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
                setPositiveButton(R.string.delete_browsing_data_prompt_allow) { dialog: DialogInterface, _ ->
                    // Use fragment's lifecycle; the view may be gone by the time dialog is interacted with.
                    lifecycleScope.launch(IO) {
                        downloadsUseCases.removeAllDownloads()
                        updatePendingDownloadToDelete(downloadStore.state.items.toSet())
                        launch(Dispatchers.Main) {
                            showSnackBar(
                                requireView(),
                                getString(R.string.download_delete_multiple_items_snackbar_1)
                            )
                        }
                    }
                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    /**
     * Schedules [items] for deletion.
     * Note: When tapping on a download item's "trash" button
     * (itemView.overflow_menu) this [items].size() will be 1.
     */
    private fun deleteDownloadItems(items: Set<DownloadItem>) {
        metrics.track(Event.DownloadsItemDeleted)

        updatePendingDownloadToDelete(items)
        undoScope = CoroutineScope(IO)
        undoScope?.allowUndo(
            requireView(),
            getMultiSelectSnackBarMessage(items),
            getString(R.string.bookmark_undo_deletion),
            onCancel = {
                undoPendingDeletion(items)
            },
            operation = getDeleteDownloadItemsOperation(downloadsUseCases, items)
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
            SpannableString(getString(R.string.download_delete_item_1)).apply {
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

        R.id.select_all_downloads_multi_select -> {
            for (items in downloadStore.state.items) {
                downloadInteractor.select(items)
            }
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    /**
     * Provides a message to the Undo snackbar.
     */
    private fun getMultiSelectSnackBarMessage(downloadItems: Set<DownloadItem>): String {
        return if (downloadItems.size > 1) {
            getString(R.string.download_delete_multiple_items_snackbar_1)
        } else {
            String.format(
                requireContext().getString(
                    R.string.download_delete_single_item_snackbar
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
            val contentLength = if (item.size.isNotEmpty()) {
                item.size.toLong()
            } else {
                0L
            }
            AbstractFetchDownloadService.openFile(
                applicationContext = it.applicationContext,
                download = DownloadState(
                    id = item.id,
                    url = item.url,
                    fileName = item.fileName,
                    contentType = item.contentType,
                    status = item.status,
                    contentLength = contentLength
                )
            )
        }

        metrics.track(Event.DownloadsItemOpened)
    }

    /**
     * Launches the coroutine to delete the provided [items].
     */
    private fun getDeleteDownloadItemsOperation(
        downloadUseCases: DownloadsUseCases,
        items: Set<DownloadItem>
    ): (suspend () -> Unit) {
        return {
            CoroutineScope(IO).launch {
                downloadStore.dispatch(DownloadFragmentAction.EnterDeletionMode)
                for (item in items) {
                    downloadUseCases.removeDownload(item.id)
                }
                downloadStore.dispatch(DownloadFragmentAction.ExitDeletionMode)
                pendingDownloadDeletionJob = null
            }
        }
    }

    /**
     * Queues the [getDeleteDownloadItemsOperation] job in [pendingDownloadDeletionJob] in case
     * the user exits the fragment and we need to quickly execute the queued deletion.
     * And adds the [items] to be deleted to the list of [DownloadFragmentStore.pendingDeletionIds],
     * which is used to determine what items to show and what items to hide from the user.
     */
    private fun updatePendingDownloadToDelete(items: Set<DownloadItem>) {
        pendingDownloadDeletionJob = getDeleteDownloadItemsOperation(downloadsUseCases, items)
        val ids = items.map { item -> item.id }.toSet()
        downloadStore.dispatch(DownloadFragmentAction.AddPendingDeletionSet(ids))
    }

    private fun undoPendingDeletion(items: Set<DownloadItem>) {
        pendingDownloadDeletionJob = null
        val ids = items.map { item -> item.id }.toSet()
        downloadStore.dispatch(DownloadFragmentAction.UndoPendingDeletionSet(ids))
    }

    /**
     * Executes pending job(s) when leaving [DownloadFragment].
     */
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
