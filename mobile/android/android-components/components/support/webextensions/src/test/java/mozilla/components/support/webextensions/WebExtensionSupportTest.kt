/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.verify

class WebExtensionSupportTest {

    @Test
    fun `sets web extension delegate on engine`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()

        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(any())
    }

    @Test
    fun `reacts to new tab being opened by adding tab to store`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onNewTab(ext, "https://mozilla.org", engineSession)
        val actionCaptor = argumentCaptor<TabListAction.AddTabAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("https://mozilla.org", actionCaptor.value.tab.content.url)
    }

    @Test
    fun `allows overriding onNewTab behaviour`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()
        var onNewTabCalled = false

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store, onNewTabOverride = { _, _, _ -> onNewTabCalled = true })
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onNewTab(ext, "https://mozilla.org", engineSession)
        assertTrue(onNewTabCalled)
    }

    @Test
    fun `reacts to new extension being installed`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val ext: WebExtension = mock()

        whenever(ext.id).thenReturn("extensionId")
        whenever(ext.url).thenReturn("url")

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onInstalled(ext)
        val actionCaptor = argumentCaptor<WebExtensionAction.InstallWebExtension>()
        verify(store).dispatch(actionCaptor.capture())
    }
}
