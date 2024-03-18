/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.controller

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.RecentTabs
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
class RecentTabControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val navController: NavController = mockk(relaxed = true)
    private val selectTabUseCase: TabsUseCases = mockk(relaxed = true)
    private val appStore: AppStore = mockk()

    private lateinit var store: BrowserStore

    private lateinit var controller: DefaultRecentTabsController

    @Before
    fun setup() {
        store = BrowserStore(
            BrowserState(),
        )
        controller = spyk(
            DefaultRecentTabsController(
                selectTabUseCase = selectTabUseCase.selectTab,
                navController = navController,
                appStore = appStore,
            ),
        )
    }

    @Test
    fun handleRecentTabClicked() {
        assertNull(RecentTabs.recentTabOpened.testGetValue())

        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }

        val tab = createTab(
            url = "https://mozilla.org",
            title = "Mozilla",
        )
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()

        controller.handleRecentTabClicked(tab.id)

        verify {
            selectTabUseCase.selectTab.invoke(tab.id)
            navController.navigate(R.id.browserFragment)
        }
        assertNotNull(RecentTabs.recentTabOpened.testGetValue())
    }

    @Test
    fun handleRecentTabClickedForMediaTab() {
        assertNull(RecentTabs.recentTabOpened.testGetValue())

        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }

        val inProgressMediaTab = createTab(
            url = "mediaUrl",
            id = "2",
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123, true),
        )

        store.dispatch(TabListAction.AddTabAction(inProgressMediaTab)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(inProgressMediaTab.id)).joinBlocking()

        controller.handleRecentTabClicked(inProgressMediaTab.id)

        verify {
            selectTabUseCase.selectTab.invoke(inProgressMediaTab.id)
            navController.navigate(R.id.browserFragment)
        }
        assertNotNull(RecentTabs.recentTabOpened.testGetValue())
    }

    @Test
    fun handleRecentTabShowAllClickedFromHome() {
        assertNull(RecentTabs.showAllClicked.testGetValue())

        controller.handleRecentTabShowAllClicked()

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_tabsTrayFragment },
            )
        }

        assertNotNull(RecentTabs.showAllClicked.testGetValue())
    }

    @Test
    fun handleRecentTabShowAllClickedFromSearchDialog() {
        assertNull(RecentTabs.showAllClicked.testGetValue())

        controller.handleRecentTabShowAllClicked()

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_tabsTrayFragment },
            )
        }

        assertNotNull(RecentTabs.showAllClicked.testGetValue())
    }
}
