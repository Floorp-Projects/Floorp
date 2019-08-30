/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class BrowserFragmentStoreTest {

    @Test
    fun bookmarkStateChange() = runBlocking {
        val initialState = defaultBrowserState()
        val store = BrowserFragmentStore(initialState)

        store.dispatch(QuickActionSheetAction.BookmarkedStateChange(true)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.quickActionSheetState.bookmarked, true)
    }

    @Test
    fun readableStateChange() = runBlocking {
        val initialState = defaultBrowserState()
        val store = BrowserFragmentStore(initialState)

        store.dispatch(QuickActionSheetAction.ReadableStateChange(true)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.quickActionSheetState.readable, true)
    }

    @Test
    fun readerActiveStateChange() = runBlocking {
        val initialState = defaultBrowserState()
        val store = BrowserFragmentStore(initialState)

        store.dispatch(QuickActionSheetAction.ReaderActiveStateChange(true)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.quickActionSheetState.readerActive, true)
    }

    @Test
    fun bounceNeededChange() = runBlocking {
        val initialState = defaultBrowserState()
        val store = BrowserFragmentStore(initialState)

        store.dispatch(QuickActionSheetAction.BounceNeededChange).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.quickActionSheetState.bounceNeeded, true)
    }

    @Test
    fun appLinkStateChange() = runBlocking {
        val initialState = defaultBrowserState()
        val store = BrowserFragmentStore(initialState)

        store.dispatch(QuickActionSheetAction.AppLinkStateChange(true)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.quickActionSheetState.isAppLink, true)
    }

    private fun defaultBrowserState(): BrowserFragmentState = BrowserFragmentState(
        quickActionSheetState = defaultQuickActionSheetState()
    )

    private fun defaultQuickActionSheetState(): QuickActionSheetState = QuickActionSheetState(
        readable = false,
        bookmarked = false,
        readerActive = false,
        bounceNeeded = false,
        isAppLink = false
    )
}
