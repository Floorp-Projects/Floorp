/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.home.recenttabs.RecentTabsListFeature

class RecentTabsListFeatureTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    @Test
    fun `GIVEN no selected tab WHEN the feature starts THEN dispatch an empty list`() {
        val browserStore = BrowserStore()
        val homeStore = HomeFragmentStore()
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            homeStore = homeStore
        )

        feature.start()

        homeStore.waitUntilIdle()

        assertEquals(0, homeStore.state.recentTabs.size)
    }

    @Test
    fun `GIVEN a selected tab WHEN the feature starts THEN dispatch the selected tab as a recent tab list`() {
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "1"
        )
        val tabs = listOf(tab)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = tabs,
                selectedTabId = "1"
            )
        )
        val homeStore = HomeFragmentStore()
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            homeStore = homeStore
        )

        feature.start()

        homeStore.waitUntilIdle()

        assertEquals(1, homeStore.state.recentTabs.size)
    }

    @Test
    fun `WHEN the browser state has an updated select tab THEN dispatch the new recent tab list`() {
        val tab1 = createTab(
            url = "https://www.mozilla.org",
            id = "1"
        )
        val tab2 = createTab(
            url = "https://www.firefox.com",
            id = "2"
        )
        val tabs = listOf(tab1, tab2)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = tabs,
                selectedTabId = "1"
            )
        )
        val homeStore = HomeFragmentStore()
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            homeStore = homeStore
        )

        feature.start()

        homeStore.waitUntilIdle()

        assertEquals(1, homeStore.state.recentTabs.size)
        assertEquals(tab1, homeStore.state.recentTabs[0])

        browserStore.dispatch(TabListAction.SelectTabAction(tab2.id)).joinBlocking()

        homeStore.waitUntilIdle()

        assertEquals(1, homeStore.state.recentTabs.size)
        assertEquals(tab2, homeStore.state.recentTabs[0])
    }

    @Test
    fun `WHEN the browser state selects a private tab THEN dispatch an empty list`() {
        val normalTab = createTab(
            url = "https://www.mozilla.org",
            id = "1"
        )
        val privateTab = createTab(
            url = "https://www.firefox.com",
            id = "2",
            private = true
        )
        val tabs = listOf(normalTab, privateTab)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = tabs,
                selectedTabId = "1"
            )
        )
        val homeStore = HomeFragmentStore()
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            homeStore = homeStore
        )

        feature.start()

        homeStore.waitUntilIdle()

        assertEquals(1, homeStore.state.recentTabs.size)
        assertEquals(normalTab, homeStore.state.recentTabs[0])

        browserStore.dispatch(TabListAction.SelectTabAction(privateTab.id)).joinBlocking()

        homeStore.waitUntilIdle()

        assertEquals(0, homeStore.state.recentTabs.size)
    }
}
