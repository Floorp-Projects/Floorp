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
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_history.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.VisitType
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getHostFromUrl
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.share.ShareTab
import java.util.concurrent.TimeUnit

@SuppressWarnings("TooManyFunctions")
class HistoryFragment : Fragment(), BackHandler {
    private lateinit var historyStore: HistoryStore
    private lateinit var historyView: HistoryView
    private lateinit var historyInteractor: HistoryInteractor

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
        historyView = HistoryView(view.history_layout, historyInteractor)
        return view
    }

    private fun invalidateOptionsMenu() {
        activity?.invalidateOptionsMenu()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        requireComponents.analytics.metrics.track(Event.HistoryOpened)
        setHasOptionsMenu(true)
    }

    fun deleteHistoryItems(items: List<HistoryItem>) {
        lifecycleScope.launch {
            val storage = context?.components?.core?.historyStorage
            for (item in items) {
                context?.components?.analytics?.metrics?.track(Event.HistoryItemRemoved)
                storage?.deleteVisit(item.url, item.visitedAt)
            }
            reloadData()
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(historyStore) {
            historyView.update(it)
        }

        lifecycleScope.launch { reloadData() }
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
            HistoryState.Mode.Normal ->
                R.menu.library_menu
            is HistoryState.Mode.Editing ->
                R.menu.history_select_multi
            else -> null
        }?.let { inflater.inflate(it, menu) }

        if (mode is HistoryState.Mode.Editing) {
            menu.findItem(R.id.share_history_multi_select)?.run {
                isVisible = mode.selectedItems.isNotEmpty()
                icon.colorFilter = PorterDuffColorFilter(
                    ContextCompat.getColor(context!!, R.color.white_color),
                    SRC_IN
                )
            }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.share_history_multi_select -> {
            val selectedHistory =
                (historyStore.state.mode as? HistoryState.Mode.Editing)?.selectedItems ?: listOf()
            when {
                selectedHistory.size == 1 ->
                    share(selectedHistory.first().url)
                selectedHistory.size > 1 -> {
                    val shareTabs = selectedHistory.map { ShareTab(it.url, it.title) }
                    share(tabs = shareTabs)
                }
            }
            true
        }
        R.id.libraryClose -> {
            Navigation.findNavController(requireActivity(), R.id.container)
                .popBackStack(R.id.libraryFragment, true)
            true
        }
        R.id.delete_history_multi_select -> {
            val components = context?.applicationContext?.components!!
            val selectedHistory =
                (historyStore.state.mode as? HistoryState.Mode.Editing)?.selectedItems ?: listOf()

            lifecycleScope.launch(Main) {
                deleteSelectedHistory(selectedHistory, components)
                reloadData()
            }
            true
        }
        R.id.open_history_in_new_tabs_multi_select -> {
            val selectedHistory =
                (historyStore.state.mode as? HistoryState.Mode.Editing)?.selectedItems ?: listOf()
            requireComponents.useCases.tabsUseCases.addTab.let { useCase ->
                for (selectedItem in selectedHistory) {
                    requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
                    useCase.invoke(selectedItem.url)
                }
            }

            (activity as HomeActivity).apply {
                browsingModeManager.mode = BrowsingModeManager.Mode.Normal
                supportActionBar?.hide()
            }
            nav(
                R.id.historyFragment,
                HistoryFragmentDirections.actionHistoryFragmentToHomeFragment()
            )
            true
        }
        R.id.open_history_in_private_tabs_multi_select -> {
            val selectedHistory =
                (historyStore.state.mode as? HistoryState.Mode.Editing)?.selectedItems ?: listOf()
            requireComponents.useCases.tabsUseCases.addPrivateTab.let { useCase ->
                for (selectedItem in selectedHistory) {
                    requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
                    useCase.invoke(selectedItem.url)
                }
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

    fun openItem(item: HistoryItem) {
        requireComponents.analytics.metrics.track(Event.HistoryItemOpened)
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = item.url,
            newTab = true,
            from = BrowserDirection.FromHistory
        )
    }

    fun displayDeleteAllDialog() {
        activity?.let { activity ->
            AlertDialog.Builder(activity).apply {
                setMessage(R.string.history_delete_all_dialog)
                setNegativeButton(android.R.string.cancel) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
                setPositiveButton(R.string.history_clear_dialog) { dialog: DialogInterface, _ ->
                    historyStore.dispatch(HistoryAction.EnterDeletionMode)
                    lifecycleScope.launch {
                        requireComponents.analytics.metrics.track(Event.HistoryAllItemsRemoved)
                        requireComponents.core.historyStorage.deleteEverything()
                        reloadData()
                        launch(Dispatchers.Main) {
                            historyStore.dispatch(HistoryAction.ExitDeletionMode)
                        }
                    }

                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    private suspend fun reloadData() {
        val excludeTypes = listOf(
            VisitType.NOT_A_VISIT,
            VisitType.DOWNLOAD,
            VisitType.REDIRECT_TEMPORARY,
            VisitType.RELOAD,
            VisitType.EMBED,
            VisitType.FRAMED_LINK,
            VisitType.REDIRECT_PERMANENT
        )

        // Until we have proper pagination, only display a limited set of history to avoid blowing up the UI.
        // See https://github.com/mozilla-mobile/fenix/issues/1393
        val sinceTimeMs = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(HISTORY_TIME_DAYS)
        val items = requireComponents.core.historyStorage
            .getDetailedVisits(sinceTimeMs, excludeTypes = excludeTypes)
            // We potentially have a large amount of visits, and multiple processing steps.
            // Wrapping iterator in a sequence should make this a little memory-more efficient.
            .asSequence()
            .sortedByDescending { it.visitTime }
            .mapIndexed { id, item ->
                val title = item.title
                    ?.takeIf(String::isNotEmpty)
                    ?: item.url.getHostFromUrl()
                    ?: item.url

                HistoryItem(id, title, item.url, item.visitTime)
            }
            .toList()

        withContext(Main) {
            historyStore.dispatch(HistoryAction.Change(items))
        }
    }

    private suspend fun deleteSelectedHistory(
        selected: List<HistoryItem>,
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

    companion object {
        private const val HISTORY_TIME_DAYS = 3L
    }
}
