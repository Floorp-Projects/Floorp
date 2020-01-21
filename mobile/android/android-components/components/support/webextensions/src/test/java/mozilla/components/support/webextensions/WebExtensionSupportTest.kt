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
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.ActionHandler
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
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
        val store = BrowserStore()

        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(any())
    }

    @Test
    fun `queries engine for installed extensions and adds state to the store`() {
        val store = spy(BrowserStore())

        val ext1: WebExtension = mock()
        val ext2: WebExtension = mock()
        whenever(ext1.id).thenReturn("1")
        whenever(ext2.id).thenReturn("2")

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(listOf(ext1, ext2))
        }

        WebExtensionSupport.initialize(engine, store)
        assertEquals(ext1, WebExtensionSupport.installedExtensions[ext1.id])
        assertEquals(ext2, WebExtensionSupport.installedExtensions[ext2.id])

        val actionCaptor = argumentCaptor<WebExtensionAction.InstallWebExtensionAction>()
        verify(store, times(2)).dispatch(actionCaptor.capture())
        assertEquals(WebExtensionState(ext1.id, ext1.url), actionCaptor.allValues[0].extension)
        assertEquals(WebExtensionState(ext2.id, ext2.url), actionCaptor.allValues[1].extension)
    }

    @Test
    fun `reacts to new tab being opened by adding tab to store`() {
        val store = spy(BrowserStore())
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onNewTab(ext, "https://mozilla.org", engineSession)
        val actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        verify(store, times(2)).dispatch(actionCaptor.capture())
        assertEquals("https://mozilla.org", (actionCaptor.allValues.first() as TabListAction.AddTabAction).tab.content.url)
        assertEquals(engineSession, (actionCaptor.allValues.last() as EngineAction.LinkEngineSessionAction).engineSession)
    }

    @Test
    fun `allows overriding onNewTab behaviour`() {
        val store = BrowserStore()
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()
        var onNewTabCalled = false

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store, onNewTabOverride = { _, _, _ ->
            onNewTabCalled = true
            "123"
        })
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
        val store = spy(BrowserStore(BrowserState(
            tabs = listOf(
                createTab(id = "1", url = "https://www.mozilla.org")
            )
        )))
        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession)).joinBlocking()

        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("extensionId")
        whenever(ext.url).thenReturn("url")
        whenever(ext.supportActions).thenReturn(true)

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Verify that we dispatch to the store and mark the extension as installed
        delegateCaptor.value.onInstalled(ext)
        verify(store).dispatch(WebExtensionAction.InstallWebExtensionAction(WebExtensionState(ext.id, ext.url)))
        assertEquals(ext, WebExtensionSupport.installedExtensions[ext.id])

        // Verify that we register an action handler for all existing sessions on the extension
        val actionHandlerCaptor = argumentCaptor<ActionHandler>()
        val actionCaptor = argumentCaptor<WebExtensionAction>()

        verify(ext).registerActionHandler(eq(engineSession), actionHandlerCaptor.capture())
        actionHandlerCaptor.value.onBrowserAction(ext, engineSession, mock())
        verify(store, times(3)).dispatch(actionCaptor.capture())
        assertEquals(ext.id, (actionCaptor.allValues.last() as WebExtensionAction.UpdateTabBrowserAction).extensionId)
    }

    @Test
    fun `reacts to install permission request`() {
        val store = spy(BrowserStore())
        val engine: Engine = mock()
        val ext: WebExtension = mock()

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Verify they we confirm the permission request
        assertTrue(delegateCaptor.value.onInstallPermissionRequest(ext))
    }

    @Test
    fun `reacts to extension being uninstalled`() {
        val store = spy(BrowserStore())

        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("extensionId")
        whenever(ext.url).thenReturn("url")
        whenever(ext.supportActions).thenReturn(true)

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onInstalled(ext)
        verify(store).dispatch(WebExtensionAction.InstallWebExtensionAction(WebExtensionState(ext.id, ext.url)))
        assertEquals(ext, WebExtensionSupport.installedExtensions[ext.id])

        // Verify that we dispatch to the store and mark the extension as uninstalled
        delegateCaptor.value.onUninstalled(ext)
        verify(store).dispatch(WebExtensionAction.UninstallWebExtensionAction(ext.id))
        assertNull(WebExtensionSupport.installedExtensions[ext.id])
    }

    @Test
    fun `reacts to extension being enabled`() {
        val store = spy(BrowserStore())

        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("extensionId")
        whenever(ext.url).thenReturn("url")
        whenever(ext.supportActions).thenReturn(true)

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onEnabled(ext)
        verify(store).dispatch(WebExtensionAction.UpdateWebExtensionEnabledAction(ext.id, true))
        assertEquals(ext, WebExtensionSupport.installedExtensions[ext.id])
    }

    @Test
    fun `reacts to extension being disabled`() {
        val store = spy(BrowserStore())

        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("extensionId")
        whenever(ext.url).thenReturn("url")
        whenever(ext.supportActions).thenReturn(true)

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onDisabled(ext)
        verify(store).dispatch(WebExtensionAction.UpdateWebExtensionEnabledAction(ext.id, false))
        assertEquals(ext, WebExtensionSupport.installedExtensions[ext.id])
    }

    @Test
    fun `observes store and registers action handlers on new engine sessions`() {
        val store = spy(BrowserStore(BrowserState(
            tabs = listOf(
                createTab(id = "1", url = "https://www.mozilla.org")
            )
        )))

        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("extensionId")
        whenever(ext.url).thenReturn("url")
        whenever(ext.supportActions).thenReturn(true)

        // Install extension
        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())
        delegateCaptor.value.onInstalled(ext)

        // Verify that action handler is registered when a new engine session is created
        val engineSession: EngineSession = mock()
        val actionHandlerCaptor = argumentCaptor<ActionHandler>()
        verify(ext, never()).registerActionHandler(any(), any())
        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession)).joinBlocking()
        verify(ext).registerActionHandler(eq(engineSession), actionHandlerCaptor.capture())
    }

    @Test
    fun `reacts to browser action being defined by dispatching to the store`() {
        val store = spy(BrowserStore())
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val browserAction: Action = mock()
        whenever(ext.id).thenReturn("test")

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onBrowserActionDefined(ext, browserAction)
        val actionCaptor = argumentCaptor<WebExtensionAction.UpdateBrowserAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("test", actionCaptor.value.extensionId)
        assertEquals(browserAction, actionCaptor.value.browserAction)
    }

    @Test
    fun `reacts to page action being defined by dispatching to the store`() {
        val store = spy(BrowserStore())
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val pageAction: Action = mock()
        whenever(ext.id).thenReturn("test")

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onPageActionDefined(ext, pageAction)
        val actionCaptor = argumentCaptor<WebExtensionAction.UpdatePageAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("test", actionCaptor.value.extensionId)
        assertEquals(pageAction, actionCaptor.value.pageAction)
    }

    @Test
    fun `reacts to action popup being toggled by opening and closing tab`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()
        val browserAction: Action = mock()
        whenever(ext.id).thenReturn("test")
        val store = spy(BrowserStore(BrowserState(
            extensions = mapOf(ext.id to WebExtensionState(ext.id))
        )))

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Toggling should open tab
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)
        var actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        verify(store, times(3)).dispatch(actionCaptor.capture())
        var values = actionCaptor.allValues
        assertEquals("", (values[0] as TabListAction.AddTabAction).tab.content.url)
        assertEquals(engineSession, (values[1] as EngineAction.LinkEngineSessionAction).engineSession)
        assertEquals("test", (values[2] as WebExtensionAction.UpdatePopupSessionAction).extensionId)

        // Update store
        store.dispatch(WebExtensionAction.UpdatePopupSessionAction(
            "test",
            (values[2] as WebExtensionAction.UpdatePopupSessionAction).popupSessionId)
        ).joinBlocking()

        val popupSessionId = store.state.extensions["test"]?.popupSessionId
        assertNotNull(popupSessionId)

        // Select different tab
        store.dispatch(TabListAction.SelectTabAction("otherTab")).joinBlocking()

        // Toggling again should select popup tab
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)
        actionCaptor = argumentCaptor()
        verify(store, times(6)).dispatch(actionCaptor.capture())
        assertEquals(popupSessionId, (actionCaptor.value as TabListAction.SelectTabAction).tabId)

        store.waitUntilIdle()

        // Toggling again should close tab
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)
        actionCaptor = argumentCaptor()
        verify(store, times(7)).dispatch(actionCaptor.capture())
        assertEquals(popupSessionId, (actionCaptor.value as TabListAction.RemoveTabAction).tabId)
    }

    @Test
    fun `allows overriding action popup behaviour`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()
        val browserAction: Action = mock()
        whenever(ext.id).thenReturn("test")
        val store = spy(BrowserStore(BrowserState(
            extensions = mapOf(ext.id to WebExtensionState(ext.id))
        )))

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()

        var webExtensionId = ""
        WebExtensionSupport.initialize(
            engine,
            store,
            onActionPopupToggleOverride = {
                webExtensionId = it.id
            }
        )
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Toggling should allow state to have popup EngineSession instance
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)
        val actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        verify(store, times(1)).dispatch(actionCaptor.capture())

        store.waitUntilIdle()
        val value = actionCaptor.value
        assertNotNull((value as WebExtensionAction.UpdatePopupSessionAction).popupSession)
        assertNotNull(store.state.extensions[webExtensionId]?.popupSession)

        // Update store to clear popupSession reference
        store.dispatch(
            WebExtensionAction.UpdatePopupSessionAction(webExtensionId, popupSession = null)
        ).joinBlocking()

        store.waitUntilIdle()
        assertNull(store.state.extensions[webExtensionId]?.popupSession)
    }

    @Test
    fun `reacts to onUpdatePermissionRequest`() {
        var executed = false
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        val store = spy(
            BrowserStore(
                BrowserState(
                    extensions = mapOf(ext.id to WebExtensionState(ext.id))
                )
            )
        )

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(
            engine = engine,
            store = store,
            onUpdatePermissionRequest = { _, _, _, _ ->
                executed = true
            })

        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())
        delegateCaptor.value.onUpdatePermissionRequest(mock(), mock(), mock(), mock())
        assertTrue(executed)
    }
}
