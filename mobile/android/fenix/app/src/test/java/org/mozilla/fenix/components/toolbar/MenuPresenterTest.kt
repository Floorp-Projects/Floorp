/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import io.mockk.clearMocks
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test

class MenuPresenterTest {

    private lateinit var store: BrowserStore
    private lateinit var testTab: TabSessionState
    private lateinit var menuPresenter: MenuPresenter
    private lateinit var menuToolbar: BrowserToolbar

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        testTab = createTab(url = "https://mozilla.org")
        store = BrowserStore(initialState = BrowserState(tabs = listOf(testTab), selectedTabId = testTab.id))
        menuToolbar = mockk(relaxed = true)
        menuPresenter = MenuPresenter(menuToolbar, store).also {
            it.start()
        }
        clearMocks(menuToolbar)
    }

    @Test
    fun `WHEN loading state is updated THEN toolbar is invalidated`() {
        verify(exactly = 0) { menuToolbar.invalidateActions() }

        store.dispatch(ContentAction.UpdateLoadingStateAction(testTab.id, true)).joinBlocking()
        verify(exactly = 1) { menuToolbar.invalidateActions() }

        store.dispatch(ContentAction.UpdateLoadingStateAction(testTab.id, false)).joinBlocking()
        verify(exactly = 2) { menuToolbar.invalidateActions() }
    }

    @Test
    fun `WHEN back navigation state is updated THEN toolbar is invalidated`() {
        verify(exactly = 0) { menuToolbar.invalidateActions() }

        store.dispatch(ContentAction.UpdateBackNavigationStateAction(testTab.id, true)).joinBlocking()
        verify(exactly = 1) { menuToolbar.invalidateActions() }

        store.dispatch(ContentAction.UpdateBackNavigationStateAction(testTab.id, false)).joinBlocking()
        verify(exactly = 2) { menuToolbar.invalidateActions() }
    }

    @Test
    fun `WHEN forward navigation state is updated THEN toolbar is invalidated`() {
        verify(exactly = 0) { menuToolbar.invalidateActions() }

        store.dispatch(ContentAction.UpdateForwardNavigationStateAction(testTab.id, true)).joinBlocking()
        verify(exactly = 1) { menuToolbar.invalidateActions() }

        store.dispatch(ContentAction.UpdateForwardNavigationStateAction(testTab.id, false)).joinBlocking()
        verify(exactly = 2) { menuToolbar.invalidateActions() }
    }

    @Test
    fun `WHEN web app manifest is updated THEN toolbar is invalidated`() {
        verify(exactly = 0) { menuToolbar.invalidateActions() }

        store.dispatch(ContentAction.UpdateWebAppManifestAction(testTab.id, mockk())).joinBlocking()
        verify(exactly = 1) { menuToolbar.invalidateActions() }
    }
}
