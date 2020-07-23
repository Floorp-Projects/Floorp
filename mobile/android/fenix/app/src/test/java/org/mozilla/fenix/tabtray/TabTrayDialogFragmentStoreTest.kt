/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.tabstray.Tab
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class TabTrayDialogFragmentStoreTest {

    @Test
    fun browserStateChange() = runBlocking {
        val initialState = emptyDefaultState()
        val store = TabTrayDialogFragmentStore(initialState)

        val newBrowserState = BrowserState(
            listOf(
                createTab("https://www.mozilla.org", id = "13256")
            )
        )

        store.dispatch(
            TabTrayDialogFragmentAction.BrowserStateChanged(
                newBrowserState
            )
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.browserState,
            newBrowserState
        )
    }

    @Test
    fun enterMultiselectMode() = runBlocking {
        val initialState = emptyDefaultState()
        val store = TabTrayDialogFragmentStore(initialState)

        store.dispatch(
            TabTrayDialogFragmentAction.EnterMultiSelectMode
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            TabTrayDialogFragmentState.Mode.MultiSelect(setOf())
        )
    }

    @Test
    fun exitMultiselectMode() = runBlocking {
        val initialState = TabTrayDialogFragmentState(
            browserState = BrowserState(),
            mode = TabTrayDialogFragmentState.Mode.MultiSelect(setOf())
        )
        val store = TabTrayDialogFragmentStore(initialState)

        store.dispatch(
            TabTrayDialogFragmentAction.ExitMultiSelectMode
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            TabTrayDialogFragmentState.Mode.Normal
        )
        assertEquals(
            store.state.mode.selectedItems,
            setOf<Tab>()
        )
    }

    @Test
    fun addItemForCollection() = runBlocking {
        val initialState = emptyDefaultState()
        val store = TabTrayDialogFragmentStore(initialState)

        val tab = Tab(id = "1234", url = "mozilla.org")
        store.dispatch(
            TabTrayDialogFragmentAction.AddItemForCollection(tab)
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            TabTrayDialogFragmentState.Mode.MultiSelect(setOf(tab))
        )
        assertEquals(
            store.state.mode.selectedItems,
            setOf(tab)
        )
    }

    @Test
    fun removeItemForCollection() = runBlocking {
        val tab = Tab(id = "1234", url = "mozilla.org")
        val secondTab = Tab(id = "12345", url = "pocket.com")

        val initialState = TabTrayDialogFragmentState(
            browserState = BrowserState(),
            mode = TabTrayDialogFragmentState.Mode.MultiSelect(setOf(tab, secondTab))
        )

        val store = TabTrayDialogFragmentStore(initialState)

        store.dispatch(
            TabTrayDialogFragmentAction.RemoveItemForCollection(tab)
        ).join()

        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            TabTrayDialogFragmentState.Mode.MultiSelect(setOf(secondTab))
        )
        assertEquals(
            store.state.mode.selectedItems,
            setOf(secondTab)
        )

        store.dispatch(
            TabTrayDialogFragmentAction.RemoveItemForCollection(secondTab)
        ).join()

        assertEquals(
            store.state.mode,
            TabTrayDialogFragmentState.Mode.Normal
        )
        assertEquals(
            store.state.mode.selectedItems,
            setOf<Tab>()
        )
    }

    private fun emptyDefaultState(): TabTrayDialogFragmentState = TabTrayDialogFragmentState(
        browserState = BrowserState(),
        mode = TabTrayDialogFragmentState.Mode.Normal
    )
}
