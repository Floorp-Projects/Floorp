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
import androidx.appcompat.widget.Toolbar
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.core.content.ContextCompat
import androidx.core.view.MenuProvider
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.downloads.DynamicDownloadDialog
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.setToolbarColors
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.allowUndo

/**
 * Fragment for displaying and managing the downloads list.
 */
@SuppressWarnings("TooManyFunctions", "LargeClass")
class DownloadFragment : Fragment(), UserInteractionHandler, MenuProvider {

    private val downloadStore by lazyStore { viewModelScope ->
        DownloadFragmentStore(
            initialState = DownloadFragmentState.INITIAL,
            middleware = listOf(
                DownloadFragmentDataMiddleware(
                    browserStore = requireComponents.core.store,
                    scope = viewModelScope,
                ),
            ),
        )
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnLifecycleDestroyed(viewLifecycleOwner))
        setContent {
            FirefoxTheme {
                DownloadsScreen(
                    downloadsStore = downloadStore,
                    onItemClick = { openItem(it) },
                    onItemDeleteClick = { deleteDownloadItems(setOf(it)) },
                )
            }
        }
    }

    private fun invalidateOptionsMenu() {
        activity?.invalidateOptionsMenu()
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

        observeModeChanges()
    }

    private fun observeModeChanges() {
        viewLifecycleOwner.lifecycleScope.launch {
            downloadStore.flow()
                .distinctUntilChangedBy { it.mode::class }
                .map { it.mode }
                .collect { mode ->
                    invalidateOptionsMenu()
                    when (mode) {
                        is DownloadFragmentState.Mode.Editing -> {
                            updateToolbarForSelectingMode(
                                title = getString(
                                    R.string.download_multi_select_title,
                                    mode.selectedItems.size,
                                ),
                            )
                        }

                        DownloadFragmentState.Mode.Normal -> {
                            updateToolbarForNormalMode(title = getString(R.string.library_downloads))
                        }
                    }
                }
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
                setTextColor(requireContext(), R.attr.textCritical)
            }
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.close_history -> {
            findNavController().popBackStack()
            true
        }

        R.id.delete_downloads_multi_select -> {
            deleteDownloadItems(downloadStore.state.mode.selectedItems)
            downloadStore.dispatch(DownloadFragmentAction.ExitEditMode)
            true
        }

        R.id.select_all_downloads_multi_select -> {
            downloadStore.dispatch(DownloadFragmentAction.AddAllItemsForRemoval)
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
                requireContext().getString(R.string.download_delete_single_item_snackbar),
                downloadItems.first().fileName,
            )
        }
    }

    override fun onBackPressed(): Boolean {
        return if (downloadStore.state.mode is DownloadFragmentState.Mode.Editing) {
            downloadStore.dispatch(DownloadFragmentAction.ExitEditMode)
            true
        } else {
            false
        }
    }

    private fun openItem(item: DownloadItem, mode: BrowsingMode? = null) {
        mode?.let { (activity as HomeActivity).browsingModeManager.mode = it }
        context?.let {
            val downloadState = DownloadState(
                id = item.id,
                url = item.url,
                fileName = item.fileName,
                contentType = item.contentType,
                status = item.status,
            )

            val canOpenFile = AbstractFetchDownloadService.openFile(
                applicationContext = it.applicationContext,
                download = downloadState,
            )

            val rootView = view
            if (!canOpenFile && rootView != null) {
                FenixSnackbar.make(
                    view = rootView,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = true,
                ).setText(
                    DynamicDownloadDialog.getCannotOpenFileErrorMessage(
                        it,
                        downloadState,
                    ),
                ).show()
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

    private fun updateToolbarForNormalMode(title: String?) {
        context?.let {
            updateToolbar(
                title = title,
                foregroundColor = it.getColorFromAttr(R.attr.textPrimary),
                backgroundColor = it.getColorFromAttr(R.attr.layer1),
            )
        }
    }

    private fun updateToolbarForSelectingMode(title: String?) {
        context?.let {
            updateToolbar(
                title = title,
                foregroundColor = ContextCompat.getColor(
                    it,
                    R.color.fx_mobile_text_color_oncolor_primary,
                ),
                backgroundColor = it.getColorFromAttr(R.attr.accent),
            )
        }
    }

    private fun updateToolbar(title: String?, foregroundColor: Int, backgroundColor: Int) {
        activity?.title = title
        val toolbar = activity?.findViewById<Toolbar>(R.id.navigationToolbar)
        toolbar?.setToolbarColors(foregroundColor, backgroundColor)
        toolbar?.setNavigationIcon(R.drawable.ic_back_button)
        toolbar?.navigationIcon?.setTint(foregroundColor)
    }

    override fun onDetach() {
        super.onDetach()
        context?.let {
            activity?.findViewById<Toolbar>(R.id.navigationToolbar)?.setToolbarColors(
                it.getColorFromAttr(R.attr.textPrimary),
                it.getColorFromAttr(R.attr.layer1),
            )
        }
    }
}
