/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Middleware
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.menu.store.BookmarkState
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.components.menu.store.copyWithBrowserMenuState

class MenuStoreTest {

    @Test
    fun `WHEN store is created THEN init action is dispatched`() {
        var initActionObserved = false
        val testMiddleware: Middleware<MenuState, MenuAction> = { _, next, action ->
            if (action == MenuAction.InitAction) {
                initActionObserved = true
            }

            next(action)
        }

        val store = MenuStore(
            initialState = MenuState(),
            middleware = listOf(testMiddleware),
        )

        // Wait for InitAction and middleware
        store.waitUntilIdle()

        assertTrue(initActionObserved)
        assertNull(store.state.browserMenuState)
    }

    @Test
    fun `GIVEN a browser menu state update WHEN copying the browser menu state THEN return the updated browser menu state`() {
        val selectedTab = TabSessionState(
            id = "tabId1",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )
        val firefoxTab = TabSessionState(
            id = "tabId2",
            content = ContentState(
                url = "www.firefox.com",
            ),
        )
        val state = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = selectedTab,
                bookmarkState = BookmarkState(),
            ),
        )

        assertEquals(selectedTab, state.browserMenuState!!.selectedTab)
        assertNull(state.browserMenuState!!.bookmarkState.guid)
        assertFalse(state.browserMenuState!!.bookmarkState.isBookmarked)

        var newState = state.copyWithBrowserMenuState {
            it.copy(selectedTab = firefoxTab)
        }

        assertEquals(firefoxTab, newState.browserMenuState!!.selectedTab)
        assertNull(state.browserMenuState!!.bookmarkState.guid)
        assertFalse(state.browserMenuState!!.bookmarkState.isBookmarked)

        val bookmarkState = BookmarkState(guid = "id", isBookmarked = true)
        newState = newState.copyWithBrowserMenuState {
            it.copy(bookmarkState = bookmarkState)
        }

        assertEquals(firefoxTab, newState.browserMenuState!!.selectedTab)
        assertEquals(bookmarkState, newState.browserMenuState!!.bookmarkState)
    }

    @Test
    fun `WHEN add bookmark action is dispatched THEN state is not updated`() = runTest {
        val initialState = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "www.google.com",
                    ),
                ),
                bookmarkState = BookmarkState(),
            ),
        )
        val store = MenuStore(initialState = initialState)

        store.dispatch(MenuAction.AddBookmark).join()

        assertEquals(initialState, store.state)
    }

    @Test
    fun `WHEN update bookmark state action is dispatched THEN bookmark state is updated`() = runTest {
        val initialState = MenuState(
            browserMenuState = BrowserMenuState(
                selectedTab = TabSessionState(
                    id = "tabId",
                    content = ContentState(
                        url = "www.google.com",
                    ),
                ),
                bookmarkState = BookmarkState(),
            ),
        )
        val store = MenuStore(initialState = initialState)

        assertNotNull(store.state.browserMenuState)
        assertNull(store.state.browserMenuState!!.bookmarkState.guid)
        assertFalse(store.state.browserMenuState!!.bookmarkState.isBookmarked)

        val newBookmarkState = BookmarkState(
            guid = "id1",
            isBookmarked = true,
        )
        store.dispatch(MenuAction.UpdateBookmarkState(bookmarkState = newBookmarkState)).join()

        assertEquals(newBookmarkState, store.state.browserMenuState!!.bookmarkState)
    }
}
