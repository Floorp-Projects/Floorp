/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

typealias WebExtensionBrowserAction = mozilla.components.concept.engine.webextension.BrowserAction

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
    fun `UninstallWebExtension - Removes all state of the uninstalled extension`() {
        val tab1 = createTab("url")
        val tab2 = createTab("url")
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab1, tab2)
            )
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
    fun `UpdateGlobalBrowserAction - Updates a browser action of an existing WebExtensionState on the BrowserState`() {
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
                tabs = listOf(tab)
            )
        )
        val mockedBrowserAction = mock<WebExtensionBrowserAction>()

        assertTrue(tab.extensionState.isEmpty())

        val extension = WebExtensionState("id", "url")

        store.dispatch(
            WebExtensionAction.UpdateTabBrowserAction(
                tab.id,
                extension.id,
                mockedBrowserAction
            )
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
                    true,
                    mockedBrowserAction1
                )
            )
        )
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab)
            )
        )

        store.dispatch(
            WebExtensionAction.UpdateTabBrowserAction(
                tab.id,
                "extensionId",
                mockedBrowserAction2
            )
        ).joinBlocking()

        val extensions = store.state.tabs.first().extensionState

        assertEquals(mockedBrowserAction2, extensions.values.first().browserAction)
    }

    @Test
    fun `UpdateBrowserActionPopupSession - Adds popup session ID to the web extension state`() {
        val store = BrowserStore()

        val extension = WebExtensionState("id", "url")
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(extension)).joinBlocking()

        assertEquals(extension, store.state.extensions[extension.id])
        assertNull(store.state.extensions[extension.id]?.browserActionPopupSession)

        store.dispatch(WebExtensionAction.UpdateBrowserActionPopupSession(extension.id, "popupId")).joinBlocking()
        assertEquals("popupId", store.state.extensions[extension.id]?.browserActionPopupSession)
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
                extensions = mapOf("id" to existingExtension)
            )
        )

        store.dispatch(WebExtensionAction.UpdateWebExtension(updatedExtension)).joinBlocking()
        assertEquals(updatedExtension, store.state.extensions.values.first())
        assertSame(updatedExtension, store.state.extensions.values.first())
    }
}
