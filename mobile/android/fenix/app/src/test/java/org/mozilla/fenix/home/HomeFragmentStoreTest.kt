/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import assertk.assertThat
import assertk.assertions.isEqualTo
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.FenixOnboarding

class HomeFragmentStoreTest {
    private lateinit var context: Context
    private lateinit var accountManager: FxaAccountManager
    private lateinit var onboarding: FenixOnboarding
    private lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var dispatchModeChanges: (mode: Mode) -> Unit
    private lateinit var currentMode: CurrentMode
    private lateinit var homeFragmentState: HomeFragmentState
    private lateinit var homeFragmentStore: HomeFragmentStore

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        accountManager = mockk(relaxed = true)
        onboarding = mockk(relaxed = true)
        browsingModeManager = mockk(relaxed = true)
        dispatchModeChanges = mockk(relaxed = true)

        every { context.components.backgroundServices.accountManager } returns accountManager
        every { onboarding.userHasBeenOnboarded() } returns true
        every { browsingModeManager.mode } returns BrowsingMode.Normal

        currentMode = CurrentMode(
            context,
            onboarding,
            browsingModeManager,
            dispatchModeChanges
        )

        homeFragmentState = HomeFragmentState(
            collections = emptyList(),
            expandedCollections = emptySet(),
            mode = currentMode.getCurrentMode(),
            tabs = emptyList(),
            topSites = emptyList()
        )

        homeFragmentStore = HomeFragmentStore(homeFragmentState)
    }

    @Test
    fun `Test toggling the mode in HomeFragmentStore`() = runBlocking {
        // Verify that the default mode and tab states of the HomeFragment are correct.
        assertThat(homeFragmentStore.state.mode).isEqualTo(Mode.Normal)
        assertEquals(0, homeFragmentStore.state.tabs.size)

        // Change the HomeFragmentStore to Private mode.
        homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(Mode.Private)).join()

        assertThat(homeFragmentStore.state.mode).isEqualTo(Mode.Private)
        assertEquals(0, homeFragmentStore.state.tabs.size)

        // Change the HomeFragmentStore back to Normal mode.
        homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(Mode.Normal)).join()

        assertThat(homeFragmentStore.state.mode).isEqualTo(Mode.Normal)
        assertEquals(0, homeFragmentStore.state.tabs.size)
    }

    @Test
    fun `Test toggling the mode with tabs in HomeFragmentStore`() = runBlocking {
        // Verify that the default mode and tab states of the HomeFragment are correct.
        assertThat(homeFragmentStore.state.mode).isEqualTo(Mode.Normal)
        assertEquals(0, homeFragmentStore.state.tabs.size)

        // Add 2 Tabs to the HomeFragmentStore.
        val tabs: List<Tab> = listOf(mockk(), mockk())
        homeFragmentStore.dispatch(HomeFragmentAction.TabsChange(tabs)).join()

        assertEquals(2, homeFragmentStore.state.tabs.size)

        // Change the HomeFragmentStore to Private mode.
        homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(Mode.Private)).join()

        assertThat(homeFragmentStore.state.mode).isEqualTo(Mode.Private)
        assertEquals(0, homeFragmentStore.state.tabs.size)
    }

    @Test
    fun `Test changing the collections in HomeFragmentStore`() = runBlocking {
        assertEquals(0, homeFragmentStore.state.collections.size)

        // Add 2 TabCollections to the HomeFragmentStore.
        val tabCollections: List<TabCollection> = listOf(mockk(), mockk())
        homeFragmentStore.dispatch(HomeFragmentAction.CollectionsChange(tabCollections)).join()

        assertThat(homeFragmentStore.state.collections).isEqualTo(tabCollections)
    }

    @Test
    fun `Test changing the top sites in HomeFragmentStore`() = runBlocking {
        assertEquals(0, homeFragmentStore.state.topSites.size)

        // Add 2 TopSites to the HomeFragmentStore.
        val topSites: List<TopSite> = listOf(mockk(), mockk())
        homeFragmentStore.dispatch(HomeFragmentAction.TopSitesChange(topSites)).join()

        assertThat(homeFragmentStore.state.topSites).isEqualTo(topSites)
    }

    @Test
    fun `Test changing the tab in HomeFragmentStore`() = runBlocking {
        assertEquals(0, homeFragmentStore.state.tabs.size)

        val tab: Tab = mockk()

        homeFragmentStore.dispatch(HomeFragmentAction.TabsChange(listOf(tab))).join()

        assertTrue(homeFragmentStore.state.tabs.contains(tab))
        assertEquals(1, homeFragmentStore.state.tabs.size)
    }

    @Test
    fun `Test changing the expanded collections in HomeFragmentStore`() = runBlocking {
        val collection: TabCollection = mockk<TabCollection>().apply {
            every { id } returns 0
        }

        // Expand the given collection.
        homeFragmentStore.dispatch(HomeFragmentAction.CollectionsChange(listOf(collection))).join()
        homeFragmentStore.dispatch(HomeFragmentAction.CollectionExpanded(collection, true)).join()

        assertTrue(homeFragmentStore.state.expandedCollections.contains(collection.id))
        assertEquals(1, homeFragmentStore.state.expandedCollections.size)
    }

    @Test
    fun `Test changing the collections, mode, tabs and top sites in the HomeFragmentStore`() = runBlocking {
        // Verify that the default state of the HomeFragment is correct.
        assertEquals(0, homeFragmentStore.state.collections.size)
        assertEquals(0, homeFragmentStore.state.tabs.size)
        assertEquals(0, homeFragmentStore.state.topSites.size)
        assertThat(homeFragmentStore.state.mode).isEqualTo(Mode.Normal)

        val collections: List<TabCollection> = listOf(mockk())
        val tabs: List<Tab> = listOf(mockk(), mockk())
        val topSites: List<TopSite> = listOf(mockk(), mockk())

        homeFragmentStore.dispatch(
            HomeFragmentAction.Change(
                collections = collections,
                mode = Mode.Private,
                tabs = tabs,
                topSites = topSites
            )
        ).join()

        assertEquals(1, homeFragmentStore.state.collections.size)
        assertThat(homeFragmentStore.state.mode).isEqualTo(Mode.Private)
        assertEquals(2, homeFragmentStore.state.tabs.size)
        assertEquals(2, homeFragmentStore.state.topSites.size)
    }
}
