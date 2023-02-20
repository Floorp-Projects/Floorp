/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.appcompat.widget.Toolbar
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.MenuProvider
import androidx.fragment.app.setFragmentResultListener
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.Orientation
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.SecureFragment
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentSavedLoginsBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.logins.LoginsAction
import org.mozilla.fenix.settings.logins.LoginsFragmentStore
import org.mozilla.fenix.settings.logins.LoginsListState
import org.mozilla.fenix.settings.logins.SavedLoginsSortingStrategyMenu
import org.mozilla.fenix.settings.logins.SortingStrategy
import org.mozilla.fenix.settings.logins.controller.LoginsListController
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.createInitialLoginsListState
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor
import org.mozilla.fenix.settings.logins.view.SavedLoginsListView

@SuppressWarnings("TooManyFunctions")
class SavedLoginsFragment : SecureFragment(), MenuProvider {
    private lateinit var savedLoginsStore: LoginsFragmentStore
    private lateinit var savedLoginsListView: SavedLoginsListView
    private lateinit var savedLoginsInteractor: SavedLoginsInteractor
    private lateinit var dropDownMenuAnchorView: View
    private lateinit var sortingStrategyMenu: SavedLoginsSortingStrategyMenu
    private lateinit var toolbarChildContainer: FrameLayout
    private lateinit var sortLoginsMenuRoot: ConstraintLayout
    private lateinit var loginsListController: LoginsListController
    private lateinit var savedLoginsStorageController: SavedLoginsStorageController
    private lateinit var loginState: LoginsListState
    private var removedLoginGuid: String? = null
    private var deletedGuid = mutableSetOf<String>()

    override fun onResume() {
        super.onResume()
        initToolbar()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        val view = inflater.inflate(R.layout.fragment_saved_logins, container, false)
        val binding = FragmentSavedLoginsBinding.bind(view)

        savedLoginsStore = StoreProvider.get(this) {
            LoginsFragmentStore(
                createInitialLoginsListState(requireContext().settings()),
            )
        }

        loginsListController =
            LoginsListController(
                loginsFragmentStore = savedLoginsStore,
                navController = findNavController(),
                browserNavigator = ::openToBrowserAndLoad,
                settings = requireContext().settings(),
            )
        savedLoginsStorageController =
            SavedLoginsStorageController(
                passwordsStorage = requireContext().components.core.passwordsStorage,
                lifecycleScope = viewLifecycleOwner.lifecycleScope,
                navController = findNavController(),
                loginsFragmentStore = savedLoginsStore,
                clipboardHandler = requireContext().components.clipboardHandler,
            )

        savedLoginsInteractor =
            SavedLoginsInteractor(
                loginsListController,
                savedLoginsStorageController,
            )

        savedLoginsListView = SavedLoginsListView(
            binding.savedLoginsLayout,
            savedLoginsInteractor,
        )
        savedLoginsInteractor.loadAndMapLogins()
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        setFragmentResultListener(LoginDetailFragment.LOGIN_REQUEST_KEY) { _, bundle ->
            removedLoginGuid = bundle.getString(LoginDetailFragment.LOGIN_BUNDLE_ARGS)
            deletedGuid.add(removedLoginGuid.toString())
        }
        consumeFrom(savedLoginsStore) { loginsListState ->
            sortingStrategyMenu.updateMenu(savedLoginsStore.state.highlightedItem)

            loginState = loginsListState
            val currentList = loginState.filteredItems.toMutableList()

            if (removedLoginGuid != null) {
                val newList = currentList.filter { !deletedGuid.contains(it.guid) }

                loginState = loginState.copy(
                    loginList = newList,
                    filteredItems = newList,
                )
            }
            savedLoginsListView.update(loginState)
        }
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.login_list, menu)
        val searchItem = menu.findItem(R.id.search)
        val searchView: SearchView = searchItem.actionView as SearchView
        searchView.imeOptions = EditorInfo.IME_ACTION_DONE
        searchView.queryHint = getString(R.string.preferences_passwords_saved_logins_search)
        searchView.maxWidth = Int.MAX_VALUE

        searchView.setOnQueryTextListener(
            object : SearchView.OnQueryTextListener {
                override fun onQueryTextSubmit(query: String?): Boolean {
                    return false
                }

                override fun onQueryTextChange(newText: String?): Boolean {
                    savedLoginsStore.dispatch(
                        LoginsAction.FilterLogins(
                            newText,
                        ),
                    )
                    return false
                }
            },
        )
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean = false

    /**
     * If we pause this fragment, we want to pop users back to reauth
     */
    override fun onPause() {
        toolbarChildContainer.removeAllViews()
        toolbarChildContainer.visibility = View.GONE
        (activity as HomeActivity).getSupportActionBarAndInflateIfNecessary()
            .setDisplayShowTitleEnabled(true)
        sortingStrategyMenu.menuController.dismiss()
        sortLoginsMenuRoot.setOnClickListener(null)

        redirectToReAuth(
            listOf(R.id.loginDetailFragment, R.id.addLoginFragment),
            findNavController().currentDestination?.id,
            R.id.savedLoginsFragment,
        )
        super.onPause()
    }

    private fun openToBrowserAndLoad(
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection,
    ) = (activity as HomeActivity).openToBrowserAndLoad(searchTermOrURL, newTab, from)

    private fun initToolbar() {
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)
        showToolbar(getString(R.string.preferences_passwords_saved_logins))
        (activity as HomeActivity).getSupportActionBarAndInflateIfNecessary()
            .setDisplayShowTitleEnabled(false)
        toolbarChildContainer = initChildContainerFromToolbar()
        sortLoginsMenuRoot = inflateSortLoginsMenuRoot()
        dropDownMenuAnchorView = sortLoginsMenuRoot.findViewById(R.id.drop_down_menu_anchor_view)
        when (requireContext().settings().savedLoginsSortingStrategy) {
            is SortingStrategy.Alphabetically -> setupMenu(
                SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort,
            )
            is SortingStrategy.LastUsed -> setupMenu(
                SavedLoginsSortingStrategyMenu.Item.LastUsedSort,
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
        sortingStrategyMenu.menuController.register(
            object : MenuController.Observer {
                override fun onDismiss() {
                    // Deactivate button on dismiss
                    sortLoginsMenuRoot.isActivated = false
                }
            },
            view = sortLoginsMenuRoot,
        )

        sortLoginsMenuRoot.setOnClickListener {
            // Activate button on show
            sortLoginsMenuRoot.isActivated = true
            sortingStrategyMenu.menuController.show(
                anchor = dropDownMenuAnchorView,
                orientation = Orientation.DOWN,
            )
        }
    }

    private fun setupMenu(itemToHighlight: SavedLoginsSortingStrategyMenu.Item) {
        sortingStrategyMenu =
            SavedLoginsSortingStrategyMenu(requireContext(), savedLoginsInteractor)
        sortingStrategyMenu.updateMenu(itemToHighlight)

        attachMenu()
    }
}
