/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.view.Menu
import android.view.MenuInflater
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.appcompat.widget.Toolbar
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_saved_logins.view.*
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.concept.storage.Login
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SupportUtils

@SuppressWarnings("TooManyFunctions")
class SavedLoginsFragment : Fragment() {
    private lateinit var savedLoginsStore: LoginsFragmentStore
    private lateinit var savedLoginsView: SavedLoginsView
    private lateinit var savedLoginsInteractor: SavedLoginsInteractor
    private lateinit var dropDownMenuAnchorView: View
    private lateinit var sortingStrategyMenu: SavedLoginsSortingStrategyMenu
    private lateinit var sortingStrategyPopupMenu: BrowserMenu
    private lateinit var toolbarChildContainer: FrameLayout
    private lateinit var sortLoginsMenuRoot: ConstraintLayout

    override fun onResume() {
        super.onResume()
        activity?.window?.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
        initToolbar()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_saved_logins, container, false)
        savedLoginsStore = StoreProvider.get(this) {
            LoginsFragmentStore(
                LoginsListState(
                    isLoading = true,
                    loginList = listOf(),
                    filteredItems = listOf(),
                    searchedForText = null,
                    sortingStrategy = requireContext().settings().savedLoginsSortingStrategy,
                    highlightedItem = requireContext().settings().savedLoginsMenuHighlightedItem
                )
            )
        }
        val savedLoginsController: SavedLoginsController =
            SavedLoginsController(savedLoginsStore, requireContext().settings())
        savedLoginsInteractor =
            SavedLoginsInteractor(savedLoginsController, ::itemClicked, ::openLearnMore)
        savedLoginsView = SavedLoginsView(view.savedLoginsLayout, savedLoginsInteractor)
        loadAndMapLogins()
        return view
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(savedLoginsStore) {
            sortingStrategyMenu.updateMenu(savedLoginsStore.state.highlightedItem)
            savedLoginsView.update(it)
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_list, menu)
        val searchItem = menu.findItem(R.id.search)
        val searchView: SearchView = searchItem.actionView as SearchView
        searchView.imeOptions = EditorInfo.IME_ACTION_DONE
        searchView.queryHint = getString(R.string.preferences_passwords_saved_logins_search)

        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String?): Boolean {
                return false
            }

            override fun onQueryTextChange(newText: String?): Boolean {
                savedLoginsStore.dispatch(LoginsAction.FilterLogins(newText))
                return false
            }
        })
    }

    /**
     * If we pause this fragment, we want to pop users back to reauth
     */
    override fun onPause() {
        toolbarChildContainer.removeAllViews()
        toolbarChildContainer.visibility = View.GONE
        (activity as HomeActivity).getSupportActionBarAndInflateIfNecessary().setDisplayShowTitleEnabled(true)
        sortingStrategyPopupMenu.dismiss()

        redirectToReAuth(listOf(R.id.loginDetailFragment), findNavController().currentDestination?.id)
        super.onPause()
    }

    private fun itemClicked(item: SavedLogin) {
        context?.components?.analytics?.metrics?.track(Event.OpenOneLogin)
        val directions =
            SavedLoginsFragmentDirections.actionSavedLoginsFragmentToLoginDetailFragment(item.guid)
        findNavController().navigate(directions)
    }

    private fun openLearnMore() {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                (SupportUtils.SumoTopic.SYNC_SETUP),
            newTab = true,
            from = BrowserDirection.FromSavedLoginsFragment
        )
    }

    private fun loadAndMapLogins() {
        var deferredLogins: Deferred<List<Login>>? = null
        val fetchLoginsJob = viewLifecycleOwner.lifecycleScope.launch(IO) {
            deferredLogins = async {
                requireContext().components.core.passwordsStorage.list()
            }
            val logins = deferredLogins?.await()
            logins?.let {
                withContext(Main) {
                    savedLoginsStore.dispatch(
                        LoginsAction.UpdateLoginsList(logins.map { it.mapToSavedLogin() })
                    )
                }
            }
        }
        fetchLoginsJob.invokeOnCompletion {
            if (it is CancellationException) {
                deferredLogins?.cancel()
            }
        }
    }

    private fun initToolbar() {
        showToolbar(getString(R.string.preferences_passwords_saved_logins))
        (activity as HomeActivity).getSupportActionBarAndInflateIfNecessary()
            .setDisplayShowTitleEnabled(false)
        toolbarChildContainer = initChildContainerFromToolbar()
        sortLoginsMenuRoot = inflateSortLoginsMenuRoot()
        dropDownMenuAnchorView = sortLoginsMenuRoot.findViewById(R.id.drop_down_menu_anchor_view)
        when (requireContext().settings().savedLoginsSortingStrategy) {
            is SortingStrategy.Alphabetically -> setupMenu(SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort)
            is SortingStrategy.LastUsed -> setupMenu(SavedLoginsSortingStrategyMenu.Item.LastUsedSort)
        }
    }

    private fun initChildContainerFromToolbar(): FrameLayout {
        val activity = activity as? AppCompatActivity
        val toolbar = (activity as HomeActivity).findViewById<Toolbar>(R.id.navigationToolbar)

        return (toolbar.findViewById(R.id.toolbar_child_container) as FrameLayout).apply {
            visibility = View.VISIBLE
        }
    }

    private fun inflateSortLoginsMenuRoot(): ConstraintLayout {
        return LayoutInflater.from(context)
            .inflate(R.layout.saved_logins_sort_items_toolbar_child, toolbarChildContainer, true)
            .findViewById(R.id.sort_logins_menu_root)
    }

    private fun attachMenu() {
        sortingStrategyPopupMenu = sortingStrategyMenu.menuBuilder.build(requireContext())

        sortLoginsMenuRoot.setOnClickListener {
            sortLoginsMenuRoot.isActivated = true
            sortingStrategyPopupMenu.show(
                anchor = dropDownMenuAnchorView,
                orientation = BrowserMenu.Orientation.DOWN
            ) {
                sortLoginsMenuRoot.isActivated = false
            }
        }
    }

    private fun setupMenu(itemToHighlight: SavedLoginsSortingStrategyMenu.Item) {
        sortingStrategyMenu = SavedLoginsSortingStrategyMenu(requireContext(), itemToHighlight) {
            when (it) {
                SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort -> {
                    savedLoginsInteractor.sort(SortingStrategy.Alphabetically(requireContext().applicationContext))
                }

                SavedLoginsSortingStrategyMenu.Item.LastUsedSort -> {
                    savedLoginsInteractor.sort(SortingStrategy.LastUsed(requireContext().applicationContext))
                }
            }
        }

        attachMenu()
    }

    companion object {
        const val SORTING_STRATEGY_ALPHABETICALLY = "ALPHABETICALLY"
        const val SORTING_STRATEGY_LAST_USED = "LAST_USED"
    }
}
