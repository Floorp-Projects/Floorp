/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TabsTrayStoreTest {

    @Test
    fun `WHEN entering select mode THEN selected tabs are empty`() {
        val store = TabsTrayStore()

        store.dispatch(TabsTrayAction.EnterSelectMode)

        store.waitUntilIdle()

        assertTrue(store.state.mode.selectedTabs.isEmpty())
        assertTrue(store.state.mode is TabsTrayState.Mode.Select)

        store.dispatch(TabsTrayAction.AddSelectTab(createTab(url = "url")))

        store.dispatch(TabsTrayAction.ExitSelectMode)
        store.dispatch(TabsTrayAction.EnterSelectMode)

        store.waitUntilIdle()

        assertTrue(store.state.mode.selectedTabs.isEmpty())
        assertTrue(store.state.mode is TabsTrayState.Mode.Select)
    }

    @Test
    fun `WHEN exiting select mode THEN the mode in the state updates`() {
        val store = TabsTrayStore()

        store.dispatch(TabsTrayAction.EnterSelectMode)

        store.waitUntilIdle()

        assertTrue(store.state.mode is TabsTrayState.Mode.Select)

        store.dispatch(TabsTrayAction.ExitSelectMode)

        store.waitUntilIdle()

        assertTrue(store.state.mode is TabsTrayState.Mode.Normal)
    }

    @Test
    fun `WHEN adding a tab to selection THEN it is added to the selectedTabs`() {
        val store = TabsTrayStore()

        store.dispatch(TabsTrayAction.AddSelectTab(createTab(url = "url", id = "tab1")))

        store.waitUntilIdle()

        assertEquals("tab1", store.state.mode.selectedTabs.take(1).first().id)
    }

    @Test
    fun `WHEN removing a tab THEN it is removed from the selectedTabs`() {
        val store = TabsTrayStore()
        val tabForRemoval = createTab(url = "url", id = "tab1")

        store.dispatch(TabsTrayAction.AddSelectTab(tabForRemoval))
        store.dispatch(TabsTrayAction.AddSelectTab(createTab(url = "url", id = "tab2")))

        store.waitUntilIdle()

        assertEquals(2, store.state.mode.selectedTabs.size)

        store.dispatch(TabsTrayAction.RemoveSelectTab(tabForRemoval))

        store.waitUntilIdle()

        assertEquals(1, store.state.mode.selectedTabs.size)
        assertEquals("tab2", store.state.mode.selectedTabs.take(1).first().id)
    }

    @Test
    fun `WHEN store is initialized THEN the default page selected in normal tabs`() {
        val store = TabsTrayStore()

        assertEquals(Page.NormalTabs, store.state.selectedPage)
    }

    @Test
    fun `WHEN page changes THEN the selectedPage is updated`() {
        val store = TabsTrayStore()

        assertEquals(Page.NormalTabs, store.state.selectedPage)

        store.dispatch(TabsTrayAction.PageSelected(Page.SyncedTabs))

        store.waitUntilIdle()

        assertEquals(Page.SyncedTabs, store.state.selectedPage)
    }

    @Test
    fun `WHEN position is converted to page THEN page is correct`() {
        assert(Page.positionToPage(0) == Page.NormalTabs)
        assert(Page.positionToPage(1) == Page.PrivateTabs)
        assert(Page.positionToPage(2) == Page.SyncedTabs)
        assert(Page.positionToPage(3) == Page.SyncedTabs)
        assert(Page.positionToPage(-1) == Page.SyncedTabs)
    }

    @Test
    fun `WHEN sync now action is triggered THEN update the sync now boolean`() {
        val store = TabsTrayStore()

        assertFalse(store.state.syncing)

        store.dispatch(TabsTrayAction.SyncNow)

        store.waitUntilIdle()

        assertTrue(store.state.syncing)
    }

    @Test
    fun `WHEN sync is complete THEN the syncing boolean is updated`() {
        val store = TabsTrayStore(initialState = TabsTrayState(syncing = true))

        assertTrue(store.state.syncing)

        store.dispatch(TabsTrayAction.SyncCompleted)

        store.waitUntilIdle()

        assertFalse(store.state.syncing)
    }
}
