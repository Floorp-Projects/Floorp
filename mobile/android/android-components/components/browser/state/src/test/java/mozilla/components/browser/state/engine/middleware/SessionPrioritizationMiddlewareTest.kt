/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.SessionPriority.DEFAULT
import mozilla.components.concept.engine.EngineSession.SessionPriority.HIGH
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.verify

class SessionPrioritizationMiddlewareTest {

    @Test
    fun `GIVEN a linked session WHEN UnlinkEngineSessionAction THEN set the DEFAULT priority to the unlinked tab`() {
        val middleware = SessionPrioritizationMiddleware()
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                ),
            ),
            middleware = listOf(middleware),
        )
        val engineSession1: EngineSession = mock()

        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession1)).joinBlocking()
        store.dispatch(EngineAction.UnlinkEngineSessionAction("1")).joinBlocking()

        verify(engineSession1).updateSessionPriority(DEFAULT)
        assertEquals("", middleware.previousHighestPriorityTabId)
    }

    @Test
    fun `GIVEN a previous selected tab WHEN LinkEngineSessionAction THEN update the selected linked tab priority to HIGH`() {
        val middleware = SessionPrioritizationMiddleware()
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                ),
            ),
            middleware = listOf(middleware),
        )
        val engineSession1: EngineSession = mock()

        store.dispatch(TabListAction.SelectTabAction("1")).joinBlocking()

        assertEquals("", middleware.previousHighestPriorityTabId)

        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession1)).joinBlocking()

        assertEquals("1", middleware.previousHighestPriorityTabId)
        verify(engineSession1).updateSessionPriority(HIGH)
    }

    @Test
    fun `GIVEN a previous selected tab with priority DEFAULT WHEN selecting and linking a new tab THEN update the previous tab to DEFAULT and the new one to HIGH`() {
        val middleware = SessionPrioritizationMiddleware()
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                    createTab("https://www.firefox.com", id = "2"),
                ),
            ),
            middleware = listOf(middleware),
        )
        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()

        store.dispatch(TabListAction.SelectTabAction("1")).joinBlocking()

        assertEquals("", middleware.previousHighestPriorityTabId)

        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession1)).joinBlocking()

        assertEquals("1", middleware.previousHighestPriorityTabId)
        verify(engineSession1).updateSessionPriority(HIGH)

        store.dispatch(TabListAction.SelectTabAction("2")).joinBlocking()

        assertEquals("1", middleware.previousHighestPriorityTabId)

        store.dispatch(EngineAction.LinkEngineSessionAction("2", engineSession2)).joinBlocking()

        assertEquals("2", middleware.previousHighestPriorityTabId)
        verify(engineSession1).updateSessionPriority(DEFAULT)
        verify(engineSession2).updateSessionPriority(HIGH)
    }

    @Test
    fun `GIVEN no linked tab WHEN SelectTabAction THEN no changes in priority show happened`() {
        val middleware = SessionPrioritizationMiddleware()
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                    createTab("https://www.firefox.com", id = "2"),
                ),
            ),
            middleware = listOf(middleware),
        )

        store.dispatch(TabListAction.SelectTabAction("1")).joinBlocking()

        assertEquals("", middleware.previousHighestPriorityTabId)
    }
}
