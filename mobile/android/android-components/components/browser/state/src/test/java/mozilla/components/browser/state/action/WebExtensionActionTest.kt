/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class WebExtensionActionTest {

    @Test
    fun `InstallWebExtension - Adds an extension to the BrowserState extensions`() {
        val store = BrowserStore()

        assertTrue(store.state.extensions.isEmpty())

        val extension = WebExtensionState("id", "url")
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()

        assertFalse(store.state.extensions.isEmpty())
        assertEquals(extension, store.state.extensions.values.first())

        // Installing the same extension twice should have no effect
        val state = store.state
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()
        assertSame(state, store.state)
    }

    @Test
    fun `InstallWebExtension - Keeps existing browser and page actions`() {
        val store = BrowserStore()
        assertTrue(store.state.extensions.isEmpty())

        val extension = WebExtensionState("id", "url", "name")
        val mockedBrowserAction = mock<WebExtensionBrowserAction>()
        val mockedPageAction = mock<WebExtensionPageAction>()
        store.dispatch(WebExtensionAction.UpdateBrowserAction(extension.id, mockedBrowserAction)).joinBlocking()
        store.dispatch(WebExtensionAction.UpdatePageAction(extension.id, mockedPageAction)).joinBlocking()
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()

        assertFalse(store.state.extensions.isEmpty())
        assertEquals(
            extension.copy(
                browserAction = mockedBrowserAction,
                pageAction = mockedPageAction,
            ),
            store.state.extensions.values.first(),
        )
    }

    @Test
    fun `UninstallWebExtension - Removes all state of the uninstalled extension`() {
        val tab1 = createTab("url")
        val tab2 = createTab("url")
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab1, tab2),
            ),
        )

        assertTrue(store.state.extensions.isEmpty())

        val extension1 = WebExtensionState("id1", "url")
        val extension2 = WebExtensionState("i2", "url")
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension1)).joinBlocking()

        assertFalse(store.state.extensions.isEmpty())
        assertEquals(extension1, store.state.extensions.values.first())

        val mockedBrowserAction = mock<WebExtensionBrowserAction>()
        store.dispatch(WebExtensionAction.UpdateBrowserAction(extension1.id, mockedBrowserAction)).joinBlocking()
        assertEquals(mockedBrowserAction, store.state.extensions.values.first().browserAction)

        store.dispatch(WebExtensionAction.UpdateTabBrowserAction(tab1.id, extension1.id, mockedBrowserAction))
            .joinBlocking()
        val extensionsTab1 = store.state.tabs.first().extensionState
        assertEquals(mockedBrowserAction, extensionsTab1.values.first().browserAction)

        store.dispatch(WebExtensionAction.UpdateTabBrowserAction(tab2.id, extension2.id, mockedBrowserAction))
            .joinBlocking()
        val extensionsTab2 = store.state.tabs.last().extensionState
        assertEquals(mockedBrowserAction, extensionsTab2.values.last().browserAction)

        store.dispatch(WebExtensionAction.UninstallWebExtensionAction(extension1.id)).joinBlocking()
        assertTrue(store.state.extensions.isEmpty())
        assertTrue(store.state.tabs.first().extensionState.isEmpty())
        assertFalse(store.state.tabs.last().extensionState.isEmpty())
        assertEquals(mockedBrowserAction, extensionsTab2.values.last().browserAction)
    }

    @Test
    fun `UninstallAllWebExtensions - Removes all state of all extensions`() {
        val tab1 = createTab("url")
        val tab2 = createTab("url")
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab1, tab2),
            ),
        )
        assertTrue(store.state.extensions.isEmpty())

        val extension1 = WebExtensionState("id1", "url")
        val extension2 = WebExtensionState("i2", "url")
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension1)).joinBlocking()
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension2)).joinBlocking()
        assertEquals(2, store.state.extensions.size)

        val mockedBrowserAction = mock<WebExtensionBrowserAction>()
        store.dispatch(WebExtensionAction.UpdateBrowserAction(extension1.id, mockedBrowserAction)).joinBlocking()
        assertEquals(mockedBrowserAction, store.state.extensions["id1"]?.browserAction)
        store.dispatch(WebExtensionAction.UpdateTabBrowserAction(tab1.id, extension1.id, mockedBrowserAction)).joinBlocking()
        store.dispatch(WebExtensionAction.UpdateTabBrowserAction(tab2.id, extension2.id, mockedBrowserAction)).joinBlocking()

        store.dispatch(WebExtensionAction.UninstallAllWebExtensionsAction).joinBlocking()
        assertTrue(store.state.extensions.isEmpty())
        assertTrue(store.state.tabs.first().extensionState.isEmpty())
        assertTrue(store.state.tabs.last().extensionState.isEmpty())
    }

    @Test
    fun `UpdateBrowserAction - Updates a global browser action of an existing WebExtensionState on the BrowserState`() {
        val store = BrowserStore()
        val mockedBrowserAction = mock<WebExtensionBrowserAction>()
        val mockedBrowserAction2 = mock<WebExtensionBrowserAction>()

        assertTrue(store.state.extensions.isEmpty())
        store.dispatch(WebExtensionAction.UpdateBrowserAction("id", mockedBrowserAction)).joinBlocking()
        assertEquals(mockedBrowserAction, store.state.extensions.values.first().browserAction)

        val extension = WebExtensionState("id", "url")
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()
        assertFalse(store.state.extensions.isEmpty())
        assertEquals(mockedBrowserAction, store.state.extensions.values.first().browserAction)

        store.dispatch(WebExtensionAction.UpdateBrowserAction("id", mockedBrowserAction2)).joinBlocking()
        assertEquals(mockedBrowserAction2, store.state.extensions.values.first().browserAction)
    }

    @Test
    fun `UpdateTabBrowserAction - Updates the browser action of an existing WebExtensionState on a given tab`() {
        val tab = createTab("url")
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
        val mockedBrowserAction = mock<WebExtensionBrowserAction>()

        assertTrue(tab.extensionState.isEmpty())

        val extension = WebExtensionState("id", "url")

        store.dispatch(
            WebExtensionAction.UpdateTabBrowserAction(
                tab.id,
                extension.id,
                mockedBrowserAction,
            ),
        ).joinBlocking()

        val extensions = store.state.tabs.first().extensionState

        assertEquals(mockedBrowserAction, extensions.values.first().browserAction)
    }

    @Test
    fun `UpdateTabBrowserAction - Updates an existing browser action`() {
        val mockedBrowserAction1 = mock<WebExtensionBrowserAction>()
        val mockedBrowserAction2 = mock<WebExtensionBrowserAction>()

        val tab = createTab(
            "url",
            extensions = mapOf(
                "extensionId" to WebExtensionState(
                    "extensionId",
                    "url",
                    "name",
                    true,
                    browserAction = mockedBrowserAction1,
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            WebExtensionAction.UpdateTabBrowserAction(
                tab.id,
                "extensionId",
                mockedBrowserAction2,
            ),
        ).joinBlocking()

        val extensions = store.state.tabs.first().extensionState

        assertEquals(mockedBrowserAction2, extensions.values.first().browserAction)
    }

    @Test
    fun `UpdatePageAction - Updates a global page action of an existing WebExtensionState on the BrowserState`() {
        val store = BrowserStore()
        val mockedPageAction = mock<WebExtensionPageAction>()
        val mockedPageAction2 = mock<WebExtensionPageAction>()

        assertTrue(store.state.extensions.isEmpty())
        store.dispatch(WebExtensionAction.UpdatePageAction("id", mockedPageAction)).joinBlocking()
        assertEquals(mockedPageAction, store.state.extensions.values.first().pageAction)

        val extension = WebExtensionState("id", "url")
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()
        assertFalse(store.state.extensions.isEmpty())
        assertEquals(mockedPageAction, store.state.extensions.values.first().pageAction)

        store.dispatch(WebExtensionAction.UpdatePageAction("id", mockedPageAction2)).joinBlocking()
        assertEquals(mockedPageAction2, store.state.extensions.values.first().pageAction)
    }

    @Test
    fun `UpdateTabPageAction - Updates the page action of an existing WebExtensionState on a given tab`() {
        val tab = createTab("url")
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
        val mockedPageAction = mock<WebExtensionPageAction>()

        assertTrue(tab.extensionState.isEmpty())

        val extension = WebExtensionState("id", "url")

        store.dispatch(
            WebExtensionAction.UpdateTabPageAction(
                tab.id,
                extension.id,
                mockedPageAction,
            ),
        ).joinBlocking()

        val extensions = store.state.tabs.first().extensionState

        assertEquals(mockedPageAction, extensions.values.first().pageAction)
    }

    @Test
    fun `UpdateTabPageAction - Updates an existing page action`() {
        val mockedPageAction1 = mock<WebExtensionPageAction>()
        val mockedPageAction2 = mock<WebExtensionPageAction>()

        val tab = createTab(
            "url",
            extensions = mapOf(
                "extensionId" to WebExtensionState(
                    "extensionId",
                    "url",
                    "name",
                    true,
                    pageAction = mockedPageAction1,
                ),
            ),
        )
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            WebExtensionAction.UpdateTabPageAction(
                tab.id,
                "extensionId",
                mockedPageAction2,
            ),
        ).joinBlocking()

        val extensions = store.state.tabs.first().extensionState

        assertEquals(mockedPageAction2, extensions.values.first().pageAction)
    }

    @Test
    fun `UpdatePopupSessionAction - Adds popup session to the web extension state`() {
        val store = BrowserStore()

        val extension = WebExtensionState("id", "url")
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()

        assertEquals(extension, store.state.extensions[extension.id])
        assertNull(store.state.extensions[extension.id]?.popupSessionId)
        assertNull(store.state.extensions[extension.id]?.popupSession)

        val engineSession: EngineSession = mock()
        store.dispatch(WebExtensionAction.UpdatePopupSessionAction(extension.id, popupSession = engineSession)).joinBlocking()
        assertEquals(engineSession, store.state.extensions[extension.id]?.popupSession)

        store.dispatch(WebExtensionAction.UpdatePopupSessionAction(extension.id, popupSession = null)).joinBlocking()
        assertNull(store.state.extensions[extension.id]?.popupSession)

        store.dispatch(WebExtensionAction.UpdatePopupSessionAction(extension.id, "popupId")).joinBlocking()
        assertEquals("popupId", store.state.extensions[extension.id]?.popupSessionId)

        store.dispatch(WebExtensionAction.UpdatePopupSessionAction(extension.id, null)).joinBlocking()
        assertNull(store.state.extensions[extension.id]?.popupSessionId)
    }

    @Test
    fun `UpdateWebExtensionEnabledAction - Updates enabled state of an existing web extension`() {
        val store = BrowserStore()
        val extension = WebExtensionState("id", "url")

        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()
        assertTrue(store.state.extensions[extension.id]?.enabled!!)

        store.dispatch(WebExtensionAction.UpdateWebExtensionEnabledAction(extension.id, false)).joinBlocking()
        assertFalse(store.state.extensions[extension.id]?.enabled!!)

        store.dispatch(WebExtensionAction.UpdateWebExtensionEnabledAction(extension.id, true)).joinBlocking()
        assertTrue(store.state.extensions[extension.id]?.enabled!!)
    }

    @Test
    fun `UpdateWebExtension -  Update an existing extension`() {
        val existingExtension = WebExtensionState("id", "url")
        val updatedExtension = WebExtensionState("id", "url2")

        val store = BrowserStore(
            initialState = BrowserState(
                extensions = mapOf("id" to existingExtension),
            ),
        )

        store.dispatch(WebExtensionAction.UpdateWebExtensionAction(updatedExtension)).joinBlocking()
        assertEquals(updatedExtension, store.state.extensions.values.first())
        assertSame(updatedExtension, store.state.extensions.values.first())
    }

    @Test
    fun `UpdateWebExtensionAllowedInPrivateBrowsingAction - Updates allowedInPrivateBrowsing state of an existing web extension`() {
        val store = BrowserStore()
        val extension = WebExtensionState("id", "url", allowedInPrivateBrowsing = false)

        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()
        assertFalse(store.state.extensions[extension.id]?.allowedInPrivateBrowsing!!)

        store.dispatch(WebExtensionAction.UpdateWebExtensionAllowedInPrivateBrowsingAction(extension.id, true)).joinBlocking()
        assertTrue(store.state.extensions[extension.id]?.allowedInPrivateBrowsing!!)

        store.dispatch(WebExtensionAction.UpdateWebExtensionAllowedInPrivateBrowsingAction(extension.id, false)).joinBlocking()
        assertFalse(store.state.extensions[extension.id]?.allowedInPrivateBrowsing!!)
    }

    @Test
    fun `UpdateWebExtensionTabAction - Marks tab active for web extensions`() {
        val tab = createTab(url = "https://mozilla.org")
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        assertNull(store.state.activeWebExtensionTabId)

        store.dispatch(WebExtensionAction.UpdateActiveWebExtensionTabAction(tab.id)).joinBlocking()
        assertEquals(tab.id, store.state.activeWebExtensionTabId)

        store.dispatch(WebExtensionAction.UpdateActiveWebExtensionTabAction(null)).joinBlocking()
        assertNull(store.state.activeWebExtensionTabId)
    }
}
