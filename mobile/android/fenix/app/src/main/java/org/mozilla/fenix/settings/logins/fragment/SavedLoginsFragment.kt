/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.appcompat.widget.Toolbar
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_saved_logins.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.Orientation
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.logins.LoginsAction
import org.mozilla.fenix.settings.logins.LoginsFragmentStore
import org.mozilla.fenix.settings.logins.SavedLoginsSortingStrategyMenu
import org.mozilla.fenix.settings.logins.SortingStrategy
import org.mozilla.fenix.settings.logins.controller.LoginsListController
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.createInitialLoginsListState
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor
import org.mozilla.fenix.settings.logins.view.SavedLoginsListView

@SuppressWarnings("TooManyFunctions")
class SavedLoginsFragment : Fragment() {
    private lateinit var savedLoginsStore: LoginsFragmentStore
    private lateinit var savedLoginsListView: SavedLoginsListView
    private lateinit var savedLoginsInteractor: SavedLoginsInteractor
    private lateinit var dropDownMenuAnchorView: View
    private lateinit var sortingStrategyMenu: SavedLoginsSortingStrategyMenu
    private lateinit var toolbarChildContainer: FrameLayout
    private lateinit var sortLoginsMenuRoot: ConstraintLayout
    private lateinit var loginsListController: LoginsListController
    private lateinit var savedLoginsStorageController: SavedLoginsStorageController

    override fun onResume() {
        super.onResume()
        activity?.window?.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
        initToolbar()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_saved_logins, container, false)
        savedLoginsStore = StoreProvider.get(this) {
            LoginsFragmentStore(
                createInitialLoginsListState(requireContext().settings())
            )
        }

        loginsListController =
            LoginsListController(
                loginsFragmentStore = savedLoginsStore,
                navController = findNavController(),
                browserNavigator = ::openToBrowserAndLoad,
                settings = requireContext().settings(),
                metrics = requireContext().components.analytics.metrics
            )
        savedLoginsStorageController =
            SavedLoginsStorageController(
                passwordsStorage = requireContext().components.core.passwordsStorage,
                lifecycleScope = viewLifecycleOwner.lifecycleScope,
                navController = findNavController(),
                loginsFragmentStore = savedLoginsStore
            )

        savedLoginsInteractor =
            SavedLoginsInteractor(
                loginsListController,
                savedLoginsStorageController
            )

        savedLoginsListView = SavedLoginsListView(
            view.savedLoginsLayout,
            savedLoginsInteractor
        )
        savedLoginsInteractor.loadAndMapLogins()
        return view
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        consumeFrom(savedLoginsStore) {
            sortingStrategyMenu.updateMenu(savedLoginsStore.state.highlightedItem)
            savedLoginsListView.update(it)
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
                savedLoginsStore.dispatch(
                    LoginsAction.FilterLogins(
                        newText
                    )
                )
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
        sortingStrategyMenu.menuController.dismiss()
        sortLoginsMenuRoot.setOnClickListener(null)
        setHasOptionsMenu(false)

        redirectToReAuth(
            listOf(R.id.loginDetailFragment),
            findNavController().currentDestination?.id,
            R.id.savedLoginsFragment
        )
        super.onPause()
    }

    private fun openToBrowserAndLoad(
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection
    ) = (activity as HomeActivity).openToBrowserAndLoad(searchTermOrURL, newTab, from)

    private fun initToolbar() {
        setHasOptionsMenu(true)
        showToolbar(getString(R.string.preferences_passwords_saved_logins))
        (activity as HomeActivity).getSupportActionBarAndInflateIfNecessary()
            .setDisplayShowTitleEnabled(false)
        toolbarChildContainer = initChildContainerFromToolbar()
        sortLoginsMenuRoot = inflateSortLoginsMenuRoot()
        dropDownMenuAnchorView = sortLoginsMenuRoot.findViewById(R.id.drop_down_menu_anchor_view)
        when (requireContext().settings().savedLoginsSortingStrategy) {
            is SortingStrategy.Alphabetically -> setupMenu(
                SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort
            )
            is SortingStrategy.LastUsed -> setupMenu(
                SavedLoginsSortingStrategyMenu.Item.LastUsedSort
            )
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
        sortingStrategyMenu.menuController.register(object : MenuController.Observer {
            override fun onDismiss() {
                // Deactivate button on dismiss
                sortLoginsMenuRoot.isActivated = false
            }
        }, view = sortLoginsMenuRoot)

        sortLoginsMenuRoot.setOnClickListener {
            // Activate button on show
            sortLoginsMenuRoot.isActivated = true
            sortingStrategyMenu.menuController.show(
                anchor = dropDownMenuAnchorView,
                orientation = Orientation.DOWN
            )
        }
    }

    private fun setupMenu(itemToHighlight: SavedLoginsSortingStrategyMenu.Item) {
        sortingStrategyMenu = SavedLoginsSortingStrategyMenu(requireContext(), savedLoginsInteractor)
        sortingStrategyMenu.updateMenu(itemToHighlight)

        attachMenu()
    }
}
