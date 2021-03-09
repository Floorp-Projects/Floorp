/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.ClipboardManager
import android.content.Context.CLIPBOARD_SERVICE
import android.content.DialogInterface
import android.os.Bundle
import android.text.SpannableString
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDirections
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_history.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.showSnackBar
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.history.createSynchronousPagedHistoryProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.utils.allowUndo

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HistoryFragment : LibraryPageFragment<HistoryItem>(), UserInteractionHandler {
    private lateinit var historyStore: HistoryFragmentStore
    private lateinit var historyInteractor: HistoryInteractor
    private lateinit var viewModel: HistoryViewModel
    private var undoScope: CoroutineScope? = null
    private var pendingHistoryDeletionJob: (suspend () -> Unit)? = null

    private var _historyView: HistoryView? = null
    protected val historyView: HistoryView
        get() = _historyView!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_history, container, false)
        historyStore = StoreProvider.get(this) {
            HistoryFragmentStore(
                HistoryFragmentState(
                    items = listOf(),
                    mode = HistoryFragmentState.Mode.Normal,
                    pendingDeletionIds = emptySet(),
                    isDeletingItems = false
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
            ::syncHistory,
            requireComponents.analytics.metrics
        )
        historyInteractor = HistoryInteractor(
            historyController
        )
        _historyView = HistoryView(
            view.historyLayout,
            historyInteractor
        )

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

        updatePendingHistoryToDelete(items)
        undoScope = CoroutineScope(IO)
        undoScope?.allowUndo(
            requireView(),
            getMultiSelectSnackBarMessage(items),
            getString(R.string.bookmark_undo_deletion),
            {
                undoPendingDeletion(items)
            },
            getDeleteHistoryItemsOperation(items)
        )
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
        if (historyStore.state.mode is HistoryFragmentState.Mode.Editing) {
            inflater.inflate(R.menu.history_select_multi, menu)
            menu.findItem(R.id.share_history_multi_select)?.isVisible = true
            menu.findItem(R.id.delete_history_multi_select)?.title =
                SpannableString(getString(R.string.bookmark_menu_delete_button)).apply {
                    setTextColor(requireContext(), R.attr.destructive)
                }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.share_history_multi_select -> {
            val selectedHistory = historyStore.state.mode.selectedItems
            val shareTabs = selectedHistory.map { ShareData(url = it.url, title = it.title) }
            share(shareTabs)
            true
        }
        R.id.delete_history_multi_select -> {
            deleteHistoryItems(historyStore.state.mode.selectedItems)
            historyStore.dispatch(HistoryFragmentAction.ExitEditMode)
            true
        }
        R.id.open_history_in_new_tabs_multi_select -> {
            openItemsInNewTab { selectedItem ->
                requireComponents.analytics.metrics.track(Event.HistoryOpenedInNewTabs)
                selectedItem.url
            }

            showTabTray()
            true
        }
        R.id.open_history_in_private_tabs_multi_select -> {
            openItemsInNewTab(private = true) { selectedItem ->
                requireComponents.analytics.metrics.track(Event.HistoryOpenedInPrivateTabs)
                selectedItem.url
            }

            (activity as HomeActivity).apply {
                browsingModeManager.mode = BrowsingMode.Private
                supportActionBar?.hide()
            }

            showTabTray()
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    private fun showTabTray() {
        invokePendingDeletion()
        findNavController().nav(
            R.id.historyFragment,
            HistoryFragmentDirections.actionGlobalTabTrayDialogFragment()
        )
    }

    private fun getMultiSelectSnackBarMessage(historyItems: Set<HistoryItem>): String {
        return if (historyItems.size > 1) {
            getString(R.string.history_delete_multiple_items_snackbar)
        } else {
            String.format(
                requireContext().getString(
                    R.string.history_delete_single_item_snackbar
                ), historyItems.first().url.toShortUrl(requireComponents.publicSuffixList)
            )
        }
    }

    override fun onPause() {
        invokePendingDeletion()
        super.onPause()
    }

    override fun onBackPressed(): Boolean {
        invokePendingDeletion()
        return historyView.onBackPressed()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _historyView = null
    }

    private fun openItem(item: HistoryItem, mode: BrowsingMode? = null) {
        when (mode?.isPrivate) {
            true -> requireComponents.analytics.metrics.track(Event.HistoryOpenedInPrivateTab)
            false -> requireComponents.analytics.metrics.track(Event.HistoryOpenedInNewTab)
            null -> requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
        }

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
                    // Use fragment's lifecycle; the view may be gone by the time dialog is interacted with.
                    lifecycleScope.launch(IO) {
                        requireComponents.analytics.metrics.track(Event.HistoryAllItemsRemoved)
                        requireComponents.core.store.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction)
                        requireComponents.core.historyStorage.deleteEverything()
                        deleteOpenTabsEngineHistory(requireComponents.core.store)
                        launch(Main) {
                            viewModel.invalidate()
                            historyStore.dispatch(HistoryFragmentAction.ExitDeletionMode)
                            showSnackBar(
                                requireView(),
                                getString(R.string.preferences_delete_browsing_data_snackbar)
                            )
                        }
                    }

                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    private suspend fun deleteOpenTabsEngineHistory(store: BrowserStore) {
        store.dispatch(EngineAction.PurgeHistoryAction).join()
    }

    private fun share(data: List<ShareData>) {
        requireComponents.analytics.metrics.track(Event.HistoryItemShared)
        val directions = HistoryFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navigate(directions)
    }

    private fun navigate(directions: NavDirections) {
        invokePendingDeletion()
        findNavController().nav(
            R.id.historyFragment,
            directions
        )
    }

    private fun getDeleteHistoryItemsOperation(items: Set<HistoryItem>): (suspend () -> Unit) {
        return {
            CoroutineScope(IO).launch {
                historyStore.dispatch(HistoryFragmentAction.EnterDeletionMode)
                context?.components?.run {
                    for (item in items) {
                        analytics.metrics.track(Event.HistoryItemRemoved)
                        core.historyStorage.deleteVisit(item.url, item.visitedAt)
                    }
                }
                historyStore.dispatch(HistoryFragmentAction.ExitDeletionMode)
                pendingHistoryDeletionJob = null
            }
        }
    }

    private fun updatePendingHistoryToDelete(items: Set<HistoryItem>) {
        pendingHistoryDeletionJob = getDeleteHistoryItemsOperation(items)
        val ids = items.map { item -> item.visitedAt }.toSet()
        historyStore.dispatch(HistoryFragmentAction.AddPendingDeletionSet(ids))
    }

    private fun undoPendingDeletion(items: Set<HistoryItem>) {
        pendingHistoryDeletionJob = null
        val ids = items.map { item -> item.visitedAt }.toSet()
        historyStore.dispatch(HistoryFragmentAction.UndoPendingDeletionSet(ids))
    }

    private fun invokePendingDeletion() {
        pendingHistoryDeletionJob?.let {
            viewLifecycleOwner.lifecycleScope.launch {
                it.invoke()
            }.invokeOnCompletion {
                pendingHistoryDeletionJob = null
            }
        }
    }

    private suspend fun syncHistory() {
        val accountManager = requireComponents.backgroundServices.accountManager
        accountManager.syncNow(SyncReason.User)
        viewModel.invalidate()
    }
}
