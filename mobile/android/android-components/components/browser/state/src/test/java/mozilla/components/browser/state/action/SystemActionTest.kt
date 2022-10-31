/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import android.content.ComponentCallbacks2
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
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
                createTab(url = "https://www.firefox.com", id = "2"),
            ),
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
                ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL,
            ),
        ).joinBlocking()

        assertNull(store.state.tabs[0].content.thumbnail)
        assertNull(store.state.tabs[1].content.thumbnail)
        // Thumbnail of selected tab should not have been removed
        assertNotNull(store.state.tabs[2].content.thumbnail)
    }
}

private fun createTabWithMockEngineSession(
    id: String,
    url: String,
) = TabSessionState(
    id,
    content = ContentState(url),
    engineState = EngineState(engineSession = mock()),
)
