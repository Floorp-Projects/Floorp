/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.DialogInterface
import android.graphics.PorterDuff.Mode.SRC_IN
import android.graphics.PorterDuffColorFilter
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import kotlinx.android.synthetic.main.fragment_history.view.*
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.history.createSynchronousPagedHistoryProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.share.ShareTab

@SuppressWarnings("TooManyFunctions")
class HistoryFragment : LibraryPageFragment<HistoryItem>(), BackHandler {
    private lateinit var historyStore: HistoryStore
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
            HistoryStore(
                HistoryState(
                    items = listOf(), mode = HistoryState.Mode.Normal
                )
            )
        }
        historyInteractor = HistoryInteractor(
            historyStore,
            ::openItem,
            ::displayDeleteAllDialog,
            ::invalidateOptionsMenu,
            ::deleteHistoryItems
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
        lifecycleScope.launch {
            context?.components?.run {
                for (item in items) {
                    analytics.metrics.track(Event.HistoryItemRemoved)
                    core.historyStorage.deleteVisit(item.url, item.visitedAt)
                }
            }
            viewModel.invalidate()
            historyStore.dispatch(HistoryAction.ExitDeletionMode)
        }
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(historyStore) {
            historyView.update(it)
        }

        viewModel.history.observe(this, Observer {
            historyView.historyAdapter.submitList(it)
        })
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).apply {
            title = getString(R.string.library_history)
            supportActionBar?.show()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        val mode = historyStore.state.mode
        when (mode) {
            HistoryState.Mode.Normal -> R.menu.library_menu
            is HistoryState.Mode.Editing -> R.menu.history_select_multi
            else -> null
        }?.let { inflater.inflate(it, menu) }

        if (mode is HistoryState.Mode.Editing) {
            menu.findItem(R.id.share_history_multi_select)?.run {
                isVisible = true
                icon.colorFilter = PorterDuffColorFilter(
                    ContextCompat.getColor(context!!, R.color.white_color),
                    SRC_IN
                )
            }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.share_history_multi_select -> {
            val selectedHistory = historyStore.state.mode.selectedItems
            when {
                selectedHistory.size == 1 ->
                    share(url = selectedHistory.first().url)
                selectedHistory.size > 1 -> {
                    val shareTabs = selectedHistory.map { ShareTab(it.url, it.title) }
                    share(tabs = shareTabs)
                }
            }
            true
        }
        R.id.libraryClose -> {
            close()
            true
        }
        R.id.delete_history_multi_select -> {
            val components = context?.components!!

            lifecycleScope.launch(Main) {
                deleteSelectedHistory(historyStore.state.mode.selectedItems, components)
                viewModel.invalidate()
                historyStore.dispatch(HistoryAction.ExitDeletionMode)
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
                HistoryFragmentDirections.actionHistoryFragmentToHomeFragment()
            )
            true
        }
        R.id.open_history_in_private_tabs_multi_select -> {
            openItemsInNewTab(private = true) { selectedItem ->
                requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
                selectedItem.url
            }

            (activity as HomeActivity).apply {
                browsingModeManager.mode = BrowsingModeManager.Mode.Private
                supportActionBar?.hide()
            }
            nav(
                R.id.historyFragment,
                HistoryFragmentDirections.actionHistoryFragmentToHomeFragment()
            )
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    override fun onBackPressed(): Boolean = historyView.onBackPressed()

    private fun openItem(item: HistoryItem) {
        requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
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
                    historyStore.dispatch(HistoryAction.EnterDeletionMode)
                    lifecycleScope.launch {
                        requireComponents.analytics.metrics.track(Event.HistoryAllItemsRemoved)
                        requireComponents.core.historyStorage.deleteEverything()
                        launch(Main) {
                            viewModel.invalidate()
                            historyStore.dispatch(HistoryAction.ExitDeletionMode)
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

    private fun share(url: String? = null, tabs: List<ShareTab>? = null) {
        requireComponents.analytics.metrics.track(Event.HistoryItemShared)
        val directions =
            HistoryFragmentDirections.actionHistoryFragmentToShareFragment(
                url = url,
                tabs = tabs?.toTypedArray()
            )
        nav(R.id.historyFragment, directions)
    }
}
