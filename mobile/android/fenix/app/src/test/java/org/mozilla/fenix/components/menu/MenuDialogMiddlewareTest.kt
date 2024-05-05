/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.menu.fake.FakeBookmarksStorage
import org.mozilla.fenix.components.menu.middleware.MenuDialogMiddleware
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore

class MenuDialogMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val bookmarksStorage: BookmarksStorage = FakeBookmarksStorage()

    @Test
    fun `GIVEN selected tab is bookmarked WHEN init action is dispatched THEN initial bookmark state is updated`() = runTest {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"

        bookmarksStorage.addItem(
            parentGuid = BookmarkRoot.Mobile.id,
            url = url,
            title = title,
            position = 5u,
        )

        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
        )

        assertFalse(store.state.browserMenuState!!.isBookmarked)

        store.dispatch(MenuAction.InitAction).join()
        store.waitUntilIdle()

        assertTrue(store.state.browserMenuState!!.isBookmarked)
    }

    @Test
    fun `GIVEN selected tab is not bookmarked WHEN init action is dispatched THEN initial bookmark state is not updated`() = runTest {
        val url = "https://www.mozilla.org"
        val title = "Mozilla"
        val browserMenuState = BrowserMenuState(
            selectedTab = createTab(
                url = url,
                title = title,
            ),
        )
        val store = createStore(
            menuState = MenuState(
                browserMenuState = browserMenuState,
            ),
        )

        assertFalse(store.state.browserMenuState!!.isBookmarked)

        store.dispatch(MenuAction.InitAction).join()
        store.waitUntilIdle()

        assertFalse(store.state.browserMenuState!!.isBookmarked)
    }

    private fun createStore(
        menuState: MenuState = MenuState(),
    ) = MenuStore(
        initialState = menuState,
        middleware = listOf(
            MenuDialogMiddleware(
                bookmarksStorage = bookmarksStorage,
                scope = scope,
            ),
        ),
    )
}
