/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.concept.tabstray.Tab
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.home.HomeFragment
import mozilla.components.browser.storage.sync.Tab as SyncTab

/**
 * [TabTrayDialogFragment] controller.
 *
 * Delegated by View Interactors, handles container business logic and operates changes on it.
 */
@Suppress("TooManyFunctions")
interface TabTrayController {
    fun handleNewTabTapped(private: Boolean)
    fun handleTabTrayDismissed()
    fun handleTabSettingsClicked()
    fun handleShareTabsOfTypeClicked(private: Boolean)
    fun handleShareSelectedTabsClicked(selectedTabs: Set<Tab>)
    fun handleSyncedTabClicked(syncTab: SyncTab)
    fun handleSaveToCollectionClicked(selectedTabs: Set<Tab>)
    fun handleBookmarkSelectedTabs(selectedTabs: Set<Tab>)
    fun handleDeleteSelectedTabs(selectedTabs: Set<Tab>)
    fun handleCloseAllTabsClicked(private: Boolean)
    fun handleBackPressed(): Boolean
    fun onModeRequested(): TabTrayDialogFragmentState.Mode
    fun handleAddSelectedTab(tab: Tab)
    fun handleRemoveSelectedTab(tab: Tab)
    fun handleOpenTab(tab: Tab)
    fun handleEnterMultiselect()
    fun handleRecentlyClosedClicked()
    fun handleSetUpAutoCloseTabsClicked()
}

/**
 * Default behavior of [TabTrayController]. Other implementations are possible.
 *
 * @param profiler [Profiler] used for profiling.
 * @param sessionManager [HomeActivity] used for retrieving a list of sessions.
 * @param browsingModeManager [HomeActivity] used for registering browsing mode.
 * @param navController [NavController] used for navigation.
 * @param dismissTabTray callback allowing to request this entire Fragment to be dismissed.
 * @param tabTrayDialogFragmentStore [TabTrayDialogFragmentStore] holding the State for all Views displayed
 * in this Controller's Fragment.
 * @param dismissTabTrayAndNavigateHome callback allowing showing an undo snackbar after tab deletion.
 * @param selectTabUseCase [TabsUseCases.SelectTabUseCase] callback allowing for selecting a tab.
 * @param registerCollectionStorageObserver callback allowing for registering the [TabCollectionStorage.Observer]
 * when needed.
 * @param showChooseCollectionDialog callback allowing saving a list of sessions to an existing collection.
 * @param showAddNewCollectionDialog callback allowing for saving a list of sessions to a new collection.
 */
@Suppress("TooManyFunctions")
class DefaultTabTrayController(
    private val activity: HomeActivity,
    private val profiler: Profiler?,
    private val sessionManager: SessionManager,
    private val browserStore: BrowserStore,
    private val browsingModeManager: BrowsingModeManager,
    private val tabCollectionStorage: TabCollectionStorage,
    private val bookmarksStorage: BookmarksStorage,
    private val scope: CoroutineScope,
    private val tabsUseCases: TabsUseCases,
    private val navController: NavController,
    private val dismissTabTray: () -> Unit,
    private val dismissTabTrayAndNavigateHome: (String) -> Unit,
    private val registerCollectionStorageObserver: () -> Unit,
    private val tabTrayDialogFragmentStore: TabTrayDialogFragmentStore,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase,
    private val showChooseCollectionDialog: (List<Session>) -> Unit,
    private val showAddNewCollectionDialog: (List<Session>) -> Unit,
    private val showUndoSnackbarForTabs: () -> Unit,
    private val showBookmarksSnackbar: () -> Unit
) : TabTrayController {

    override fun handleNewTabTapped(private: Boolean) {
        val startTime = profiler?.getProfilerTime()
        browsingModeManager.mode = BrowsingMode.fromBoolean(private)
        navController.navigate(TabTrayDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true))
        dismissTabTray()
        profiler?.addMarker(
            "DefaultTabTrayController.onNewTabTapped",
            startTime
        )
    }

    override fun handleTabSettingsClicked() {
        navController.navigate(TabTrayDialogFragmentDirections.actionGlobalTabSettingsFragment())
    }

    override fun handleTabTrayDismissed() {
        dismissTabTray()
    }

    override fun handleSaveToCollectionClicked(selectedTabs: Set<Tab>) {
        val sessionList = selectedTabs.map {
            sessionManager.findSessionById(it.id) ?: return
        }

        // Only register the observer right before moving to collection creation
        registerCollectionStorageObserver()

        when {
            tabCollectionStorage.cachedTabCollections.isNotEmpty() -> {
                showChooseCollectionDialog(sessionList)
            }
            else -> {
                showAddNewCollectionDialog(sessionList)
            }
        }
    }

    override fun handleShareTabsOfTypeClicked(private: Boolean) {
        val tabs = browserStore.state.getNormalOrPrivateTabs(private)
        val data = tabs.map {
            ShareData(url = it.content.url, title = it.content.title)
        }
        val directions = TabTrayDialogFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navController.navigate(directions)
    }

    override fun handleShareSelectedTabsClicked(selectedTabs: Set<Tab>) {
        val data = selectedTabs.map {
            ShareData(url = it.url, title = it.title)
        }
        val directions = TabTrayDialogFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navController.navigate(directions)
    }

    override fun handleBookmarkSelectedTabs(selectedTabs: Set<Tab>) {
        selectedTabs.forEach {
            scope.launch(IO) {
                val shouldAddBookmark = bookmarksStorage.getBookmarksWithUrl(it.url)
                    .firstOrNull { it.url == it.url } == null
                if (shouldAddBookmark) {
                    bookmarksStorage.addItem(
                        BookmarkRoot.Mobile.id,
                        url = it.url,
                        title = it.title,
                        position = null
                    )
                }
            }
        }
        tabTrayDialogFragmentStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
        showBookmarksSnackbar()
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun handleDeleteSelectedTabs(selectedTabs: Set<Tab>) {
        if (browserStore.state.normalTabs.size == selectedTabs.size) {
            dismissTabTrayAndNavigateHome(HomeFragment.ALL_NORMAL_TABS)
        } else {
            selectedTabs.map { it.id }.let {
                tabsUseCases.removeTabs(it)
            }

            tabTrayDialogFragmentStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
            showUndoSnackbarForTabs()
        }
    }

    override fun handleSyncedTabClicked(syncTab: SyncTab) {
        activity.openToBrowserAndLoad(
            searchTermOrURL = syncTab.active().url,
            newTab = true,
            from = BrowserDirection.FromTabTray
        )
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun handleCloseAllTabsClicked(private: Boolean) {
        val sessionsToClose = if (private) {
            HomeFragment.ALL_PRIVATE_TABS
        } else {
            HomeFragment.ALL_NORMAL_TABS
        }

        dismissTabTrayAndNavigateHome(sessionsToClose)
    }

    override fun handleAddSelectedTab(tab: Tab) {
        tabTrayDialogFragmentStore.dispatch(TabTrayDialogFragmentAction.AddItemForCollection(tab))
    }

    override fun handleRemoveSelectedTab(tab: Tab) {
        tabTrayDialogFragmentStore.dispatch(TabTrayDialogFragmentAction.RemoveItemForCollection(tab))
    }

    override fun handleBackPressed(): Boolean {
        return if (tabTrayDialogFragmentStore.state.mode is TabTrayDialogFragmentState.Mode.MultiSelect) {
            tabTrayDialogFragmentStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
            true
        } else {
            false
        }
    }

    override fun onModeRequested(): TabTrayDialogFragmentState.Mode {
        return tabTrayDialogFragmentStore.state.mode
    }

    override fun handleOpenTab(tab: Tab) {
        selectTabUseCase.invoke(tab.id)
    }

    override fun handleEnterMultiselect() {
        tabTrayDialogFragmentStore.dispatch(TabTrayDialogFragmentAction.EnterMultiSelectMode)
    }

    override fun handleRecentlyClosedClicked() {
        val directions = TabTrayDialogFragmentDirections.actionGlobalRecentlyClosed()
        navController.navigate(directions)
    }

    override fun handleSetUpAutoCloseTabsClicked() {
        val directions = TabTrayDialogFragmentDirections.actionGlobalTabSettingsFragment()
        navController.navigate(directions)
    }
}
