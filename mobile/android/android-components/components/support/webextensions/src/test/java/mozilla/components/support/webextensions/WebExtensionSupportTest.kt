/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebExtensionSupportTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

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
    fun `reacts to tab being closed by removing tab from store`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()
        val invalidEngineSession: EngineSession = mock()
        val tabId = "testTabId"
        val store = spy(BrowserStore(BrowserState(
            tabs = listOf(
                createTab(id = tabId, url = "https://www.mozilla.org")
            )
        )))
        store.dispatch(EngineAction.LinkEngineSessionAction(tabId, engineSession)).joinBlocking()

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onCloseTab(ext, invalidEngineSession)
        verify(store, never()).dispatch(TabListAction.RemoveTabAction(tabId))

        delegateCaptor.value.onCloseTab(ext, engineSession)
        verify(store).dispatch(TabListAction.RemoveTabAction(tabId))
    }

    @Test
    fun `allows overriding onCloseTab behaviour`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()
        var onCloseTabCalled = false
        val tabId = "testTabId"
        val store = spy(BrowserStore(BrowserState(
            tabs = listOf(
                createTab(id = tabId, url = "https://www.mozilla.org")
            )
        )))
        store.dispatch(EngineAction.LinkEngineSessionAction(tabId, engineSession)).joinBlocking()

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store, onCloseTabOverride = { _, _ -> onCloseTabCalled = true })
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onCloseTab(ext, engineSession)
        assertTrue(onCloseTabCalled)
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
