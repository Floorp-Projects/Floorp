/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.tabstray.Tab
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.home.HomeFragment

/**
 * An interactor that helps with navigating to different parts of the app from the tabs tray.
 */
interface NavigationInteractor {

    /**
     * Called when tab tray should be dismissed.
     */
    fun onTabTrayDismissed()

    /**
     * Called when sharing a list of [Tab]s.
     */
    fun onShareTabs(tabs: Collection<Tab>)

    /**
     * Called when clicking the share tabs button.
     */
    fun onShareTabsOfTypeClicked(private: Boolean)

    /**
     * Called when clicking the tab settings button.
     */
    fun onTabSettingsClicked()

    /**
     * Called when clicking the close all tabs button.
     */
    fun onCloseAllTabsClicked(private: Boolean)

    /**
     * Called when opening the recently closed tabs menu button.
     */
    fun onOpenRecentlyClosedClicked()

    /**
     * Used when opening the add-to-collections user flow.
     */
    fun onSaveToCollections(tabs: Collection<Tab>)

    /**
     * Used when adding [Tab]s as bookmarks.
     */
    fun onSaveToBookmarks(tabs: Collection<Tab>)
}

/**
 * A default implementation of [NavigationInteractor].
 */
@Suppress("LongParameterList")
class DefaultNavigationInteractor(
    private val tabsTrayStore: TabsTrayStore,
    private val browserStore: BrowserStore,
    private val navController: NavController,
    private val metrics: MetricController,
    private val dismissTabTray: () -> Unit,
    private val dismissTabTrayAndNavigateHome: (String) -> Unit,
    private val bookmarksUseCase: BookmarksUseCase
) : NavigationInteractor {

    override fun onTabTrayDismissed() {
        dismissTabTray()
    }

    override fun onTabSettingsClicked() {
        navController.navigate(TabsTrayFragmentDirections.actionGlobalTabSettingsFragment())
    }

    override fun onOpenRecentlyClosedClicked() {
        navController.navigate(TabsTrayFragmentDirections.actionGlobalRecentlyClosed())
        metrics.track(Event.RecentlyClosedTabsOpened)
    }

    override fun onShareTabs(tabs: Collection<Tab>) {
        val data = tabs.map {
            ShareData(url = it.url, title = it.title)
        }
        val directions = TabsTrayFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navController.navigate(directions)
    }

    override fun onShareTabsOfTypeClicked(private: Boolean) {
        val tabs = browserStore.state.getNormalOrPrivateTabs(private)
        val data = tabs.map {
            ShareData(url = it.content.url, title = it.content.title)
        }
        val directions = TabsTrayFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navController.navigate(directions)
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun onCloseAllTabsClicked(private: Boolean) {
        val sessionsToClose = if (private) {
            HomeFragment.ALL_PRIVATE_TABS
        } else {
            HomeFragment.ALL_NORMAL_TABS
        }

        dismissTabTrayAndNavigateHome(sessionsToClose)
    }

    override fun onSaveToCollections(tabs: Collection<Tab>) {
        metrics.track(Event.TabsTraySaveToCollectionPressed)

        // TODO add this is a separate PR; it's quite a large change.
    }

    override fun onSaveToBookmarks(tabs: Collection<Tab>) {
        tabs.forEach { tab ->
            // We don't combine the context with lifecycleScope so that our jobs are not cancelled
            // if we leave the fragment, i.e. we still want the bookmarks to be added.
            CoroutineScope(Dispatchers.IO).launch {
                bookmarksUseCase.addBookmark(tab.url, tab.title)
            }
        }

        tabsTrayStore.dispatch(TabsTrayAction.ExitSelectMode)

        // TODO show successful snackbar here (regardless of operation succes).
    }
}
