/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.search.SearchRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

private const val SELECTED_TAB_ID = "1"
private const val CUSTOM_TAB_ID = "2"

class BrowserStoreSearchAdapterTest {

    private lateinit var browserStore: BrowserStore
    private val state = BrowserState(
        tabs = listOf(createTab(id = SELECTED_TAB_ID, url = "https://mozilla.org", private = true)),
        customTabs = listOf(createCustomTab(id = CUSTOM_TAB_ID, url = "https://firefox.com", source = SessionState.Source.Internal.CustomTab)),
        selectedTabId = SELECTED_TAB_ID,
    )

    @Before
    fun setup() {
        browserStore = mock()
        whenever(browserStore.state).thenReturn(state)
    }

    @Test
    fun `adapter does nothing with null tab`() {
        whenever(browserStore.state).thenReturn(BrowserState())
        val searchAdapter = BrowserStoreSearchAdapter(browserStore)

        searchAdapter.sendSearch(isPrivate = false, text = "normal search")
        searchAdapter.sendSearch(isPrivate = true, text = "private search")

        verify(browserStore, never()).dispatch(any())
        assertFalse(searchAdapter.isPrivateSession())
    }

    @Test
    fun `sendSearch with selected tab`() {
        val searchAdapter = BrowserStoreSearchAdapter(browserStore)
        searchAdapter.sendSearch(isPrivate = false, text = "normal search")
        verify(browserStore).dispatch(
            ContentAction.UpdateSearchRequestAction(
                SELECTED_TAB_ID,
                SearchRequest(isPrivate = false, query = "normal search"),
            ),
        )

        searchAdapter.sendSearch(isPrivate = true, text = "private search")
        verify(browserStore).dispatch(
            ContentAction.UpdateSearchRequestAction(
                SELECTED_TAB_ID,
                SearchRequest(isPrivate = true, query = "private search"),
            ),
        )

        assertTrue(searchAdapter.isPrivateSession())
    }

    @Test
    fun `sendSearch with custom tab`() {
        val searchAdapter = BrowserStoreSearchAdapter(browserStore, CUSTOM_TAB_ID)
        searchAdapter.sendSearch(isPrivate = false, text = "normal search")
        verify(browserStore).dispatch(
            ContentAction.UpdateSearchRequestAction(
                CUSTOM_TAB_ID,
                SearchRequest(isPrivate = false, query = "normal search"),
            ),
        )

        searchAdapter.sendSearch(isPrivate = true, text = "private search")
        verify(browserStore).dispatch(
            ContentAction.UpdateSearchRequestAction(
                CUSTOM_TAB_ID,
                SearchRequest(isPrivate = true, query = "private search"),
            ),
        )

        assertFalse(searchAdapter.isPrivateSession())
    }
}
