/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.text.SpannableString
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.navigation.fragment.findNavController
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentRecentlyClosedTabsBinding
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.LibraryPageFragment

@Suppress("TooManyFunctions")
class RecentlyClosedFragment : LibraryPageFragment<RecoverableTab>(), UserInteractionHandler {
    private lateinit var recentlyClosedFragmentStore: RecentlyClosedFragmentStore
    private var _recentlyClosedFragmentView: RecentlyClosedFragmentView? = null
    protected val recentlyClosedFragmentView: RecentlyClosedFragmentView
        get() = _recentlyClosedFragmentView!!

    private lateinit var recentlyClosedInteractor: RecentlyClosedFragmentInteractor
    private lateinit var recentlyClosedController: RecentlyClosedController

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.library_recently_closed_tabs))
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        if (recentlyClosedFragmentStore.state.selectedTabs.isNotEmpty()) {
            inflater.inflate(R.menu.history_select_multi, menu)
            menu.findItem(R.id.delete_history_multi_select)?.let { deleteItem ->
                deleteItem.title = SpannableString(deleteItem.title)
                    .apply { setTextColor(requireContext(), R.attr.destructive) }
            }
        } else {
            inflater.inflate(R.menu.library_menu, menu)
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        val selectedTabs = recentlyClosedFragmentStore.state.selectedTabs

        return when (item.itemId) {
            R.id.close_history -> {
                close()
                true
            }
            R.id.share_history_multi_select -> {
                recentlyClosedController.handleShare(selectedTabs)
                true
            }
            R.id.delete_history_multi_select -> {
                recentlyClosedController.handleDelete(selectedTabs)
                true
            }
            R.id.open_history_in_new_tabs_multi_select -> {
                recentlyClosedController.handleOpen(selectedTabs, BrowsingMode.Normal)
                true
            }
            R.id.open_history_in_private_tabs_multi_select -> {
                recentlyClosedController.handleOpen(selectedTabs, BrowsingMode.Private)
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val binding = FragmentRecentlyClosedTabsBinding.inflate(inflater, container, false)
        recentlyClosedFragmentStore = StoreProvider.get(this) {
            RecentlyClosedFragmentStore(
                RecentlyClosedFragmentState(
                    items = listOf(),
                    selectedTabs = emptySet()
                )
            )
        }
        recentlyClosedController = DefaultRecentlyClosedController(
            navController = findNavController(),
            browserStore = requireComponents.core.store,
            recentlyClosedStore = recentlyClosedFragmentStore,
            activity = activity as HomeActivity,
            tabsUseCases = requireComponents.useCases.tabsUseCases,
            resources = requireContext().resources,
            snackbar = FenixSnackbar.make(
                view = requireActivity().getRootView()!!,
                isDisplayedWithBrowserToolbar = true
            ),
            clipboardManager = activity?.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager,
            openToBrowser = ::openItem
        )
        recentlyClosedInteractor = RecentlyClosedFragmentInteractor(recentlyClosedController)
        _recentlyClosedFragmentView = RecentlyClosedFragmentView(
            binding.recentlyClosedLayout,
            recentlyClosedInteractor
        )
        return binding.root
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _recentlyClosedFragmentView = null
    }

    private fun openItem(tab: RecoverableTab, mode: BrowsingMode? = null) {
        mode?.let { (activity as HomeActivity).browsingModeManager.mode = it }

        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = tab.url,
            newTab = true,
            from = BrowserDirection.FromRecentlyClosed
        )
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        consumeFrom(recentlyClosedFragmentStore) { state ->
            recentlyClosedFragmentView.update(state)
            activity?.invalidateOptionsMenu()
        }

        requireComponents.core.store.flowScoped(viewLifecycleOwner) { flow ->
            flow.map { state -> state.closedTabs }
                .ifChanged()
                .collect { tabs ->
                    recentlyClosedFragmentStore.dispatch(
                        RecentlyClosedFragmentAction.Change(tabs)
                    )
                }
        }
    }

    override val selectedItems: Set<RecoverableTab> = setOf()

    override fun onBackPressed(): Boolean {
        return recentlyClosedController.handleBackPressed()
    }
}
