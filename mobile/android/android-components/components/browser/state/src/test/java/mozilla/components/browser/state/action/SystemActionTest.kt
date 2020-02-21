/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class SystemActionTest {

    @Test
    fun `LowMemoryAction removes thumbnails`() {
        val initialState = BrowserState(
            tabs = listOf(
                createTab(url = "https://www.mozilla.org", id = "0"),
                createTab(url = "https://www.firefox.com", id = "1"),
                createTab(url = "https://www.firefox.com", id = "2")
            )
        )
        val store = BrowserStore(initialState)

        store.dispatch(ContentAction.UpdateThumbnailAction("0", mock())).joinBlocking()
        store.dispatch(ContentAction.UpdateThumbnailAction("1", mock())).joinBlocking()
        store.dispatch(ContentAction.UpdateThumbnailAction("2", mock())).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tabId = "2")).joinBlocking()
        assertNotNull(store.state.tabs[0].content.thumbnail)
        assertNotNull(store.state.tabs[1].content.thumbnail)
        assertNotNull(store.state.tabs[2].content.thumbnail)

        store.dispatch(
            SystemAction.LowMemoryAction(
                states = emptyMap()
            )
        ).joinBlocking()

        assertNull(store.state.tabs[0].content.thumbnail)
        assertNull(store.state.tabs[1].content.thumbnail)
        // Thumbnail of selected tab should not have been removed
        assertNotNull(store.state.tabs[2].content.thumbnail)
    }

    @Test
    fun `LowMemoryAction removes EngineSession references and adds state`() {
        val initialState = BrowserState(
            tabs = listOf(
                createTabWithMockEngineSession(url = "https://www.mozilla.org", id = "0"),
                createTabWithMockEngineSession(url = "https://www.firefox.com", id = "1"),
                createTabWithMockEngineSession(url = "https://www.firefox.com", id = "2")
            ),
            selectedTabId = "1"
        )
        val store = BrowserStore(initialState)

        val state0: EngineSessionState = mock()
        val state2: EngineSessionState = mock()

        store.state.tabs[0].apply {
            assertNotNull(engineState.engineSession)
            assertNull(engineState.engineSessionState)
        }

        store.state.tabs[1].apply {
            assertNotNull(engineState.engineSession)
            assertNull(engineState.engineSessionState)
        }

        store.state.tabs[2].apply {
            assertNotNull(engineState.engineSession)
            assertNull(engineState.engineSessionState)
        }

        store.dispatch(
            SystemAction.LowMemoryAction(
                states = mapOf(
                    "0" to state0,
                    "2" to state2
                )
            )
        ).joinBlocking()

        store.state.tabs[0].apply {
            assertNull(engineState.engineSession)
            assertNotNull(engineState.engineSessionState)
        }

        store.state.tabs[1].apply {
            assertNotNull(engineState.engineSession)
            assertNull(engineState.engineSessionState)
        }

        store.state.tabs[2].apply {
            assertNull(engineState.engineSession)
            assertNotNull(engineState.engineSessionState)
        }
    }
}

private fun createTabWithMockEngineSession(
    id: String,
    url: String
) = TabSessionState(
    id,
    content = ContentState(url),
    engineState = EngineState(engineSession = mock())
)
