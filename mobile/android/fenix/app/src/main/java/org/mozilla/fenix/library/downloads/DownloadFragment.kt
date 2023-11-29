/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import android.content.Context
import android.os.Bundle
import android.text.SpannableString
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.core.view.MenuProvider
import androidx.lifecycle.Lifecycle
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.components
import org.mozilla.fenix.databinding.FragmentDownloadsBinding
import org.mozilla.fenix.downloads.DynamicDownloadDialog
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.filterNotExistsOnDisk
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.utils.allowUndo

@SuppressWarnings("TooManyFunctions", "LargeClass")
class DownloadFragment : LibraryPageFragment<DownloadItem>(), UserInteractionHandler, MenuProvider {
    private lateinit var downloadStore: DownloadFragmentStore
    private lateinit var downloadView: DownloadView
    private lateinit var downloadInteractor: DownloadInteractor

    private var _binding: FragmentDownloadsBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentDownloadsBinding.inflate(inflater, container, false)

        val items = provideDownloads(requireComponents.core.store.state)

        downloadStore = StoreProvider.get(this) {
            DownloadFragmentStore(
                DownloadFragmentState(
                    items = items,
                    mode = DownloadFragmentState.Mode.Normal,
                    pendingDeletionIds = emptySet(),
                    isDeletingItems = false,
                ),
            )
        }
        val downloadController: DownloadController = DefaultDownloadController(
            downloadStore,
            ::openItem,
            ::invalidateOptionsMenu,
            ::deleteDownloadItems,
        )
        downloadInteractor = DownloadInteractor(
            downloadController,
        )
        downloadView = DownloadView(binding.downloadsLayout, downloadInteractor)

        return binding.root
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    /**
     * Returns a list of available downloads to be displayed to the user.
     * Downloads must be COMPLETED and existent on disk.
     */
    @VisibleForTesting
    internal fun provideDownloads(state: BrowserState): List<DownloadItem> {
        return state.downloads.values
            .distinctBy { it.fileName }
            .sortedByDescending { it.createdTime } // sort from newest to oldest
            .map {
                DownloadItem(
                    id = it.id,
                    url = it.url,
                    fileName = it.fileName,
                    filePath = it.filePath,
                    size = it.contentLength?.toString() ?: "0",
                    contentType = it.contentType,
                    status = it.status,
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
    }

    /**
     * Schedules [items] for deletion.
     * Note: When tapping on a download item's "trash" button
     * (itemView.overflow_menu) this [items].size() will be 1.
     */
    private fun deleteDownloadItems(items: Set<DownloadItem>) {
        updatePendingDownloadToDelete(items)
        MainScope().allowUndo(
            requireActivity().getRootView()!!,
            getMultiSelectSnackBarMessage(items),
            getString(R.string.bookmark_undo_deletion),
            onCancel = {
                undoPendingDeletion(items)
            },
            operation = getDeleteDownloadItemsOperation(items),
        )
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        consumeFrom(downloadStore) {
            downloadView.update(it)
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.library_downloads))
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        val menuRes = when (downloadStore.state.mode) {
            is DownloadFragmentState.Mode.Normal -> R.menu.library_menu
            is DownloadFragmentState.Mode.Editing -> R.menu.download_select_multi
        }
        inflater.inflate(menuRes, menu)

        menu.findItem(R.id.delete_downloads_multi_select)?.title =
            SpannableString(getString(R.string.download_delete_item_1)).apply {
                setTextColor(requireContext(), R.attr.textWarning)
            }
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
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
        // other options are not handled by this menu provider
        else -> false
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
                    R.string.download_delete_single_item_snackbar,
                ),
                downloadItems.first().fileName,
            )
        }
    }

    override fun onBackPressed(): Boolean {
        return downloadView.onBackPressed()
    }

    private fun openItem(item: DownloadItem) {
        context?.let {
            val contentLength = if (item.size.isNotEmpty()) {
                item.size.toLong()
            } else {
                0L
            }
            val downloadState = DownloadState(
                id = item.id,
                url = item.url,
                fileName = item.fileName,
                contentType = item.contentType,
                status = item.status,
                contentLength = contentLength,
            )

            val canOpenFile = AbstractFetchDownloadService.openFile(
                applicationContext = it.applicationContext,
                download = downloadState,
            )

            if (!canOpenFile) {
                FenixSnackbar.make(
                    view = binding.root,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = true,
                ).setText(
                    DynamicDownloadDialog.getCannotOpenFileErrorMessage(
                        it,
                        downloadState,
                    ),
                )
                    .show()
            }
        }
    }

    private fun getDeleteDownloadItemsOperation(
        items: Set<DownloadItem>,
    ): (suspend (context: Context) -> Unit) {
        return { context ->
            CoroutineScope(IO).launch {
                downloadStore.dispatch(DownloadFragmentAction.EnterDeletionMode)
                context.let {
                    for (item in items) {
                        it.components.useCases.downloadUseCases.removeDownload(item.id)
                    }
                }
                downloadStore.dispatch(DownloadFragmentAction.ExitDeletionMode)
            }
        }
    }

    private fun updatePendingDownloadToDelete(items: Set<DownloadItem>) {
        val ids = items.map { item -> item.id }.toSet()
        downloadStore.dispatch(DownloadFragmentAction.AddPendingDeletionSet(ids))
    }

    private fun undoPendingDeletion(items: Set<DownloadItem>) {
        val ids = items.map { item -> item.id }.toSet()
        downloadStore.dispatch(DownloadFragmentAction.UndoPendingDeletionSet(ids))
    }
}
