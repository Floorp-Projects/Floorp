/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.navigation.NavController
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
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
     * Called when user clicks the share tabs button.
     */
    fun onShareTabsOfTypeClicked(private: Boolean)

    /**
     * Called when user clicks the tab settings button.
     */
    fun onTabSettingsClicked()

    /**
     * Called when user clicks the close all tabs button.
     */
    fun onCloseAllTabsClicked(private: Boolean)

    /**
     * Called when user clicks the recently closed tabs menu button.
     */
    fun onOpenRecentlyClosedClicked()
}

/**
 * A default implementation of [NavigationInteractor].
 */
class DefaultNavigationInteractor(
    private val browserStore: BrowserStore,
    private val navController: NavController,
    private val metrics: MetricController,
    private val dismissTabTray: () -> Unit,
    private val dismissTabTrayAndNavigateHome: (String) -> Unit
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
}
