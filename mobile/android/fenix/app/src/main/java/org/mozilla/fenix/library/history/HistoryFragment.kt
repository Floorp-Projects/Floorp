/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.ClipboardManager
import android.content.Context.CLIPBOARD_SERVICE
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_history.view.*
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.showSnackBar
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.history.createSynchronousPagedHistoryProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.library.LibraryPageFragment

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HistoryFragment : LibraryPageFragment<HistoryItem>(), UserInteractionHandler {
    private lateinit var historyStore: HistoryFragmentStore
    private lateinit var historyView: HistoryView
    private lateinit var historyInteractor: HistoryInteractor
    private lateinit var viewModel: HistoryViewModel

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_history, container, false)
        historyStore = StoreProvider.get(this) {
            HistoryFragmentStore(
                HistoryFragmentState(
                    items = listOf(), mode = HistoryFragmentState.Mode.Normal
                )
            )
        }
        val historyController: HistoryController = DefaultHistoryController(
            historyStore,
            findNavController(),
            resources,
            FenixSnackbar.make(
                view = view,
                duration = FenixSnackbar.LENGTH_LONG,
                isDisplayedWithBrowserToolbar = false
            ),
            activity?.getSystemService(CLIPBOARD_SERVICE) as ClipboardManager,
            lifecycleScope,
            ::openItem,
            ::displayDeleteAllDialog,
            ::invalidateOptionsMenu,
            ::deleteHistoryItems,
            ::syncHistory
        )
        historyInteractor = HistoryInteractor(
            historyController
        )
        historyView = HistoryView(view.historyLayout, historyInteractor)

        return view
    }

    override val selectedItems get() = historyStore.state.mode.selectedItems

    private fun invalidateOptionsMenu() {
        activity?.invalidateOptionsMenu()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        viewModel = HistoryViewModel(
            requireComponents.core.historyStorage.createSynchronousPagedHistoryProvider()
        )

        viewModel.userHasHistory.observe(this, Observer {
            historyView.updateEmptyState(it)
        })

        requireComponents.analytics.metrics.track(Event.HistoryOpened)

        setHasOptionsMenu(true)
    }

    private fun deleteHistoryItems(items: Set<HistoryItem>) {
        val message = getMultiSelectSnackBarMessage(items)
        viewLifecycleOwner.lifecycleScope.launch {
            context?.components?.run {
                for (item in items) {
                    analytics.metrics.track(Event.HistoryItemRemoved)
                    core.historyStorage.deleteVisit(item.url, item.visitedAt)
                }
            }
            viewModel.invalidate()
            showSnackBar(requireView(), message)
            historyStore.dispatch(HistoryFragmentAction.ExitDeletionMode)
        }
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(historyStore) {
            historyView.update(it)
        }

        viewModel.history.observe(viewLifecycleOwner, Observer {
            historyView.historyAdapter.submitList(it)
        })
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.library_history))
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        val menuRes = when (historyStore.state.mode) {
            HistoryFragmentState.Mode.Normal -> R.menu.library_menu
            is HistoryFragmentState.Mode.Editing -> R.menu.history_select_multi
            else -> return
        }

        inflater.inflate(menuRes, menu)
        menu.findItem(R.id.share_history_multi_select)?.isVisible = true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.share_history_multi_select -> {
            val selectedHistory = historyStore.state.mode.selectedItems
            val shareTabs = selectedHistory.map { ShareData(url = it.url, title = it.title) }
            share(shareTabs)
            true
        }
        R.id.delete_history_multi_select -> {
            val message = getMultiSelectSnackBarMessage(selectedItems)
            viewLifecycleOwner.lifecycleScope.launch(Main) {
                deleteSelectedHistory(historyStore.state.mode.selectedItems, requireComponents)
                viewModel.invalidate()
                historyStore.dispatch(HistoryFragmentAction.ExitDeletionMode)
                showSnackBar(requireView(), message)
            }
            true
        }
        R.id.open_history_in_new_tabs_multi_select -> {
            openItemsInNewTab { selectedItem ->
                requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
                selectedItem.url
            }

            nav(
                R.id.historyFragment,
                HistoryFragmentDirections.actionGlobalHome()
            )
            true
        }
        R.id.open_history_in_private_tabs_multi_select -> {
            openItemsInNewTab(private = true) { selectedItem ->
                requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
                selectedItem.url
            }

            (activity as HomeActivity).apply {
                browsingModeManager.mode = BrowsingMode.Private
                supportActionBar?.hide()
            }
            nav(
                R.id.historyFragment,
                HistoryFragmentDirections.actionGlobalHome()
            )
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    private fun getMultiSelectSnackBarMessage(historyItems: Set<HistoryItem>): String {
        return if (historyItems.size > 1) {
            getString(R.string.history_delete_multiple_items_snackbar)
        } else {
            getString(
                R.string.history_delete_single_item_snackbar,
                historyItems.first().url.toShortUrl(requireComponents.publicSuffixList)
            )
        }
    }

    override fun onBackPressed(): Boolean = historyView.onBackPressed()

    private fun openItem(item: HistoryItem, mode: BrowsingMode? = null) {
        requireComponents.analytics.metrics.track(Event.HistoryItemOpened)

        mode?.let { (activity as HomeActivity).browsingModeManager.mode = it }

        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = item.url,
            newTab = true,
            from = BrowserDirection.FromHistory
        )
    }

    private fun displayDeleteAllDialog() {
        activity?.let { activity ->
            AlertDialog.Builder(activity).apply {
                setMessage(R.string.delete_browsing_data_prompt_message)
                setNegativeButton(R.string.delete_browsing_data_prompt_cancel) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
                setPositiveButton(R.string.delete_browsing_data_prompt_allow) { dialog: DialogInterface, _ ->
                    historyStore.dispatch(HistoryFragmentAction.EnterDeletionMode)
                    viewLifecycleOwner.lifecycleScope.launch {
                        requireComponents.analytics.metrics.track(Event.HistoryAllItemsRemoved)
                        requireComponents.core.historyStorage.deleteEverything()
                        launch(Main) {
                            viewModel.invalidate()
                            historyStore.dispatch(HistoryFragmentAction.ExitDeletionMode)
                            showSnackBar(requireView(), getString(R.string.preferences_delete_browsing_data_snackbar))
                        }
                    }

                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    private suspend fun deleteSelectedHistory(
        selected: Set<HistoryItem>,
        components: Components = requireComponents
    ) {
        requireComponents.analytics.metrics.track(Event.HistoryItemRemoved)
        val storage = components.core.historyStorage
        for (item in selected) {
            storage.deleteVisit(item.url, item.visitedAt)
        }
    }

    private fun share(data: List<ShareData>) {
        requireComponents.analytics.metrics.track(Event.HistoryItemShared)
        val directions = HistoryFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        nav(R.id.historyFragment, directions)
    }

    private suspend fun syncHistory() {
        val accountManager = requireComponents.backgroundServices.accountManager
        accountManager.syncNowAsync(SyncReason.User).await()
        viewModel.invalidate()
    }
}
