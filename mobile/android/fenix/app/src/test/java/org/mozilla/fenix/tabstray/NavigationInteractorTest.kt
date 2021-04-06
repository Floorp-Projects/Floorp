/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.metrics.MetricController

class NavigationInteractorTest {
    private lateinit var store: BrowserStore
    private lateinit var navigationInteractor: NavigationInteractor
    private val testTab: TabSessionState = createTab(url = "https://mozilla.org")
    private val navController: NavController = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val dismissTabTray: () -> Unit = mockk(relaxed = true)
    private val dismissTabTrayAndNavigateHome: (String) -> Unit = mockk(relaxed = true)

    @Before
    fun setup() {
        store = BrowserStore(initialState = BrowserState(tabs = listOf(testTab)))
        navigationInteractor = DefaultNavigationInteractor(
            store,
            navController,
            metrics,
            dismissTabTray,
            dismissTabTrayAndNavigateHome
        )
    }

    @Test
    fun `navigation interactor calls the overridden functions`() {
        var tabTrayDismissed = false
        var tabSettingsClicked = false
        var openRecentlyClosedClicked = false
        var shareTabsOfTypeClicked = false
        var closeAllTabsClicked = false

        class TestNavigationInteractor : NavigationInteractor {

            override fun onTabTrayDismissed() {
                tabTrayDismissed = true
            }

            override fun onTabSettingsClicked() {
                tabSettingsClicked = true
            }

            override fun onOpenRecentlyClosedClicked() {
                openRecentlyClosedClicked = true
            }

            override fun onShareTabsOfTypeClicked(private: Boolean) {
                shareTabsOfTypeClicked = true
            }

            override fun onCloseAllTabsClicked(private: Boolean) {
                closeAllTabsClicked = true
            }
        }

        val navigationInteractor: NavigationInteractor = TestNavigationInteractor()
        navigationInteractor.onTabTrayDismissed()
        assert(tabTrayDismissed)
        navigationInteractor.onTabSettingsClicked()
        assert(tabSettingsClicked)
        navigationInteractor.onOpenRecentlyClosedClicked()
        assert(openRecentlyClosedClicked)
        navigationInteractor.onShareTabsOfTypeClicked(true)
        assert(shareTabsOfTypeClicked)
        navigationInteractor.onCloseAllTabsClicked(true)
        assert(closeAllTabsClicked)
    }

    @Test
    fun `onTabTrayDismissed calls dismissTabTray on DefaultNavigationInteractor`() {
        navigationInteractor.onTabTrayDismissed()
        verify(exactly = 1) { dismissTabTray() }
    }

    @Test
    fun `onTabSettingsClicked calls navigation on DefaultNavigationInteractor`() {
        navigationInteractor.onTabSettingsClicked()
        verify(exactly = 1) { navController.navigate(TabsTrayFragmentDirections.actionGlobalTabSettingsFragment()) }
    }

    @Test
    fun `onOpenRecentlyClosedClicked calls navigation on DefaultNavigationInteractor`() {
        navigationInteractor.onOpenRecentlyClosedClicked()
        verify(exactly = 1) { navController.navigate(TabsTrayFragmentDirections.actionGlobalRecentlyClosed()) }
    }

    @Test
    fun `onCloseAllTabsClicked calls navigation on DefaultNavigationInteractor`() {
        navigationInteractor.onCloseAllTabsClicked(false)
        verify(exactly = 1) { dismissTabTrayAndNavigateHome(any()) }
    }

    @Test
    fun `onShareTabsOfType calls navigation on DefaultNavigationInteractor`() {
        navigationInteractor.onShareTabsOfTypeClicked(false)
        verify(exactly = 1) { navController.navigate(any<NavDirections>()) }
    }
}
