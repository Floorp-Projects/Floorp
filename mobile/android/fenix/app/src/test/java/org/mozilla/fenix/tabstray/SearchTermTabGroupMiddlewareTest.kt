/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.action.TabGroupAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.lib.state.MiddlewareContext
import org.junit.Before
import org.junit.Test

class SearchTermTabGroupMiddlewareTest {

    private lateinit var store: BrowserStore
    private lateinit var searchTermTabGroupMiddleware: SearchTermTabGroupMiddleware

    @Before
    fun setUp() {
        searchTermTabGroupMiddleware = SearchTermTabGroupMiddleware()
        store = BrowserStore(
            middleware = listOf(searchTermTabGroupMiddleware) + EngineMiddleware.create(engine = mockk()),
            initialState = BrowserState()
        )
    }

    @Test
    fun `WHEN invoking with set history metadata key action THEN dispatch add tab action`() {
        val context: MiddlewareContext<BrowserState, BrowserAction> = mockk()
        val next: (BrowserAction) -> Unit = {}

        every { context.dispatch(any()) } returns Unit

        searchTermTabGroupMiddleware.invoke(
            context,
            next,
            HistoryMetadataAction.SetHistoryMetadataKeyAction("tabId", HistoryMetadataKey("url", "search term", "url"))
        )

        verify { context.dispatch(TabGroupAction.AddTabAction(SEARCH_TERM_TAB_GROUPS, "search term", "tabId")) }
    }

    @Test
    fun `WHEN invoking with disband search group action THEN dispatch remove tab group action`() {
        val context: MiddlewareContext<BrowserState, BrowserAction> = mockk()
        val next: (BrowserAction) -> Unit = {}
        val state: BrowserState = mockk()
        val tabPartitions =
            mapOf(Pair(SEARCH_TERM_TAB_GROUPS, TabPartition(SEARCH_TERM_TAB_GROUPS, listOf(TabGroup("testId", "search term", listOf("tab1"))))))

        every { context.dispatch(any()) } returns Unit
        every { context.state } returns state
        every { state.tabPartitions } returns tabPartitions

        searchTermTabGroupMiddleware.invoke(
            context,
            next,
            HistoryMetadataAction.DisbandSearchGroupAction("search term")
        )

        verify { context.dispatch(TabGroupAction.RemoveTabGroupAction(SEARCH_TERM_TAB_GROUPS, "testId")) }
    }

    @Test
    fun `WHEN invoking with restore action THEN dispatch add tab action`() {
        val context: MiddlewareContext<BrowserState, BrowserAction> = mockk()
        val next: (BrowserAction) -> Unit = {}

        every { context.dispatch(any()) } returns Unit

        searchTermTabGroupMiddleware.invoke(
            context,
            next,
            TabListAction.RestoreAction(
                listOf(
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(
                            id = "testId",
                            url = "url",
                            historyMetadata = HistoryMetadataKey("url", "search term", "url")
                        )
                    )
                ),
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.BEGINNING
            )
        )

        verify { context.dispatch(TabGroupAction.AddTabAction(SEARCH_TERM_TAB_GROUPS, "search term", "testId")) }
    }
}
