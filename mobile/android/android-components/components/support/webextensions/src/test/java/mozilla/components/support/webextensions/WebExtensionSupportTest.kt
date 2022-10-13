/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.engine.webextension.ActionHandler
import mozilla.components.concept.engine.webextension.Metadata
import mozilla.components.concept.engine.webextension.TabHandler
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionDelegate
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import mozilla.components.support.webextensions.WebExtensionSupport.toState
import mozilla.components.support.webextensions.facts.WebExtensionFacts.Items.WEB_EXTENSIONS_INITIALIZED
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import mozilla.components.support.base.facts.Action as FactsAction

@RunWith(AndroidJUnit4::class)
class WebExtensionSupportTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @After
    fun tearDown() {
        WebExtensionSupport.installedExtensions.clear()
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
        val ext1Meta: Metadata = mock()
        whenever(ext1Meta.name).thenReturn("ext1")
        val ext2: WebExtension = mock()
        whenever(ext1.id).thenReturn("1")
        whenever(ext1.url).thenReturn("url1")
        whenever(ext1.getMetadata()).thenReturn(ext1Meta)
        whenever(ext1.isEnabled()).thenReturn(true)
        whenever(ext1.isAllowedInPrivateBrowsing()).thenReturn(true)

        whenever(ext2.id).thenReturn("2")
        whenever(ext2.url).thenReturn("url2")
        whenever(ext2.isEnabled()).thenReturn(false)
        whenever(ext2.isAllowedInPrivateBrowsing()).thenReturn(false)

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(listOf(ext1, ext2))
        }

        CollectionProcessor.withFactCollection { facts ->
            WebExtensionSupport.initialize(engine, store)

            val interactionFact = facts[0]
            assertEquals(FactsAction.INTERACTION, interactionFact.action)
            assertEquals(Component.SUPPORT_WEBEXTENSIONS, interactionFact.component)
            assertEquals(WEB_EXTENSIONS_INITIALIZED, interactionFact.item)
            assertEquals(2, interactionFact.metadata?.size)
            assertTrue(interactionFact.metadata?.containsKey("installed")!!)
            assertTrue(interactionFact.metadata?.containsKey("enabled")!!)
            assertEquals(listOf(ext1.id, ext2.id), interactionFact.metadata?.get("installed"))
            assertEquals(listOf(ext1.id), interactionFact.metadata?.get("enabled"))
        }
        assertEquals(ext1, WebExtensionSupport.installedExtensions[ext1.id])
        assertEquals(ext2, WebExtensionSupport.installedExtensions[ext2.id])

        val actionCaptor = argumentCaptor<WebExtensionAction.InstallWebExtensionAction>()
        verify(store, times(2)).dispatch(actionCaptor.capture())
        assertEquals(
            WebExtensionState(ext1.id, ext1.url, "ext1", true, true),
            actionCaptor.allValues[0].extension,
        )
        assertEquals(
            WebExtensionState(ext2.id, ext2.url, null, false, false),
            actionCaptor.allValues[1].extension,
        )
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

        delegateCaptor.value.onNewTab(ext, engineSession, true, "https://mozilla.org")
        val actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        verify(store, times(2)).dispatch(actionCaptor.capture())
        assertEquals(
            "https://mozilla.org",
            (actionCaptor.allValues.first() as TabListAction.AddTabAction).tab.content.url,
        )
        assertEquals(
            engineSession,
            (actionCaptor.allValues.last() as EngineAction.LinkEngineSessionAction).engineSession,
        )
    }

    @Test
    fun `allows overriding onNewTab behaviour`() {
        val store = BrowserStore()
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        val engineSession: EngineSession = mock()
        var onNewTabCalled = false

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(
            engine,
            store,
            onNewTabOverride = { _, _, _ ->
                onNewTabCalled = true
                "123"
            },
        )
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onNewTab(ext, engineSession, true, "https://mozilla.org")
        assertTrue(onNewTabCalled)
    }

    @Test
    fun `reacts to tab being closed by removing tab from store`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        whenever(ext.isEnabled()).thenReturn(true)
        whenever(ext.hasTabHandler(any())).thenReturn(false, true)
        val engineSession: EngineSession = mock()
        val tabId = "testTabId"
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(id = tabId, url = "https://www.mozilla.org", engineSession = engineSession),
                    ),
                ),
            ),
        )
        val installedList = mutableListOf(ext)
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(installedList)
        }

        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        WebExtensionSupport.initialize(engine, store)

        store.waitUntilIdle()
        verify(ext).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())
        tabHandlerCaptor.value.onCloseTab(ext, engineSession)
        verify(store).dispatch(TabListAction.RemoveTabAction(tabId))
    }

    @Test
    fun `reacts to custom tab being closed by removing tab from store`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        whenever(ext.isEnabled()).thenReturn(true)
        whenever(ext.hasTabHandler(any())).thenReturn(false, true)
        val engineSession: EngineSession = mock()
        val tabId = "testTabId"
        val store = spy(
            BrowserStore(
                BrowserState(
                    customTabs = listOf(
                        createCustomTab(id = tabId, url = "https://www.mozilla.org", engineSession = engineSession, source = SessionState.Source.Internal.CustomTab),
                    ),
                ),
            ),
        )
        val installedList = mutableListOf(ext)
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(installedList)
        }

        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        WebExtensionSupport.initialize(engine, store)

        store.waitUntilIdle()
        verify(ext).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())
        tabHandlerCaptor.value.onCloseTab(ext, engineSession)
        verify(store).dispatch(CustomTabListAction.RemoveCustomTabAction(tabId))
    }

    @Test
    fun `allows overriding onCloseTab behaviour`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        whenever(ext.isEnabled()).thenReturn(true)
        whenever(ext.hasTabHandler(any())).thenReturn(false, true)
        val engineSession: EngineSession = mock()
        var onCloseTabCalled = false
        val tabId = "testTabId"
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(id = tabId, url = "https://www.mozilla.org", engineSession = engineSession),
                    ),
                ),
            ),
        )

        val installedList = mutableListOf(ext)
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(installedList)
        }

        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        WebExtensionSupport.initialize(
            engine,
            store,
            onSelectTabOverride = { _, _ -> },
            onCloseTabOverride = { _, _ -> onCloseTabCalled = true },
        )

        store.waitUntilIdle()
        verify(ext).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())
        tabHandlerCaptor.value.onCloseTab(ext, engineSession)
        assertTrue(onCloseTabCalled)
    }

    @Test
    fun `reacts to tab being updated`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        whenever(ext.isEnabled()).thenReturn(true)
        whenever(ext.hasTabHandler(any())).thenReturn(false, true)
        val engineSession: EngineSession = mock()
        val tabId = "testTabId"
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = tabId, url = "https://www.mozilla.org", engineSession = engineSession),
                ),
            ),
        )

        val installedList = mutableListOf(ext)
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(installedList)
        }

        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        WebExtensionSupport.initialize(engine, store)

        // Update tab to select it
        verify(ext).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())
        assertNull(store.state.selectedTabId)
        assertTrue(tabHandlerCaptor.value.onUpdateTab(ext, engineSession, true, null))
        store.waitUntilIdle()
        assertEquals("testTabId", store.state.selectedTabId)

        // Update URL of tab
        assertTrue(tabHandlerCaptor.value.onUpdateTab(ext, engineSession, false, "url"))
        verify(engineSession).loadUrl("url")

        // Update non-existing tab
        store.dispatch(TabListAction.RemoveTabAction(tabId)).joinBlocking()
        assertFalse(tabHandlerCaptor.value.onUpdateTab(ext, engineSession, true, "url"))
    }

    @Test
    fun `reacts to custom tab being updated`() {
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        whenever(ext.isEnabled()).thenReturn(true)
        whenever(ext.hasTabHandler(any())).thenReturn(false, true)
        val engineSession: EngineSession = mock()
        val tabId = "testTabId"
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(
                    createCustomTab(id = tabId, url = "https://www.mozilla.org", engineSession = engineSession, source = SessionState.Source.Internal.CustomTab),
                ),
            ),
        )

        val installedList = mutableListOf(ext)
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(installedList)
        }

        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        WebExtensionSupport.initialize(engine, store)

        // Update tab to select it
        verify(ext).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())
        assertNull(store.state.selectedTabId)
        assertTrue(tabHandlerCaptor.value.onUpdateTab(ext, engineSession, true, null))
        store.waitUntilIdle()

        // Update URL of tab
        assertTrue(tabHandlerCaptor.value.onUpdateTab(ext, engineSession, false, "url"))
        verify(engineSession).loadUrl("url")

        // Update non-existing tab
        store.dispatch(CustomTabListAction.RemoveCustomTabAction(tabId)).joinBlocking()
        assertFalse(tabHandlerCaptor.value.onUpdateTab(ext, engineSession, true, "url"))
    }

    @Test
    fun `reacts to new extension being installed`() {
        val engineSession: EngineSession = mock()
        val tab =
            createTab(id = "1", url = "https://www.mozilla.org", engineSession = engineSession)

        val customTabEngineSession: EngineSession = mock()
        val customTab =
            createCustomTab(id = "2", url = "https://www.mozilla.org", engineSession = customTabEngineSession, source = SessionState.Source.Internal.CustomTab)

        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    customTabs = listOf(customTab),
                ),
            ),
        )

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
        verify(store).dispatch(
            WebExtensionAction.InstallWebExtensionAction(
                WebExtensionState(ext.id, ext.url, ext.getMetadata()?.name, ext.isEnabled()),
            ),
        )
        assertEquals(ext, WebExtensionSupport.installedExtensions[ext.id])

        // Verify that we register action and tab handlers for all existing sessions on the extension
        val actionHandlerCaptor = argumentCaptor<ActionHandler>()
        val webExtensionActionCaptor = argumentCaptor<WebExtensionAction>()
        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        val selectTabActionCaptor = argumentCaptor<TabListAction.SelectTabAction>()
        verify(ext).registerActionHandler(eq(customTabEngineSession), actionHandlerCaptor.capture())
        verify(ext).registerTabHandler(eq(customTabEngineSession), tabHandlerCaptor.capture())
        verify(ext).registerActionHandler(eq(engineSession), actionHandlerCaptor.capture())
        verify(ext).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())

        // Verify we only register the handlers once
        whenever(ext.hasActionHandler(engineSession)).thenReturn(true)
        whenever(ext.hasTabHandler(engineSession)).thenReturn(true)

        store.dispatch(ContentAction.UpdateUrlAction(sessionId = "1", url = "https://www.firefox.com")).joinBlocking()
        verify(ext, times(1)).registerActionHandler(eq(engineSession), actionHandlerCaptor.capture())
        verify(ext, times(1)).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())

        actionHandlerCaptor.value.onBrowserAction(ext, engineSession, mock())
        verify(store, times(3)).dispatch(webExtensionActionCaptor.capture())
        assertEquals(ext.id, (webExtensionActionCaptor.allValues.last() as WebExtensionAction.UpdateTabBrowserAction).extensionId)

        reset(store)

        tabHandlerCaptor.value.onUpdateTab(ext, engineSession, true, null)
        verify(store).dispatch(selectTabActionCaptor.capture())
        assertEquals("1", selectTabActionCaptor.value.tabId)
        tabHandlerCaptor.value.onUpdateTab(ext, engineSession, true, "url")
        verify(engineSession).loadUrl("url")
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
        verify(store).dispatch(
            WebExtensionAction.InstallWebExtensionAction(
                WebExtensionState(ext.id, ext.url, ext.getMetadata()?.name, ext.isEnabled()),
            ),
        )
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
    fun `observes store and registers handlers on new engine sessions`() {
        val tab = createTab(id = "1", url = "https://www.mozilla.org")
        val customTab = createCustomTab(id = "2", url = "https://www.mozilla.org", source = SessionState.Source.Internal.CustomTab)
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    customTabs = listOf(customTab),
                ),
            ),
        )

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

        // Verify that action/tab handler is registered when a new engine session is created
        val actionHandlerCaptor = argumentCaptor<ActionHandler>()
        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        verify(ext, never()).registerActionHandler(any(), any())
        verify(ext, never()).registerTabHandler(
            session = any(),
            tabHandler = any(),
        )

        val engineSession1: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession1)).joinBlocking()
        verify(ext).registerActionHandler(eq(engineSession1), actionHandlerCaptor.capture())
        verify(ext).registerTabHandler(eq(engineSession1), tabHandlerCaptor.capture())

        val engineSession2: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(customTab.id, engineSession2)).joinBlocking()
        verify(ext).registerActionHandler(eq(engineSession2), actionHandlerCaptor.capture())
        verify(ext).registerTabHandler(eq(engineSession2), tabHandlerCaptor.capture())
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
    fun `reacts to action popup being toggled by opening tab as needed`() {
        val engine: Engine = mock()

        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")

        val engineSession: EngineSession = mock()
        val browserAction: Action = mock()
        val store = spy(
            BrowserStore(
                BrowserState(
                    extensions = mapOf(ext.id to WebExtensionState(ext.id)),
                ),
            ),
        )

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store, openPopupInTab = true)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Toggling should open tab
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)
        var actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        verify(store, times(3)).dispatch(actionCaptor.capture())
        var values = actionCaptor.allValues
        assertEquals("", (values[0] as TabListAction.AddTabAction).tab.content.url)
        assertEquals(engineSession, (values[1] as EngineAction.LinkEngineSessionAction).engineSession)
        assertEquals("test", (values[2] as WebExtensionAction.UpdatePopupSessionAction).extensionId)
        val popupSessionId = (values[2] as WebExtensionAction.UpdatePopupSessionAction).popupSessionId
        assertNotNull(popupSessionId)
    }

    @Test
    fun `reacts to action popup being toggled by selecting tab as needed`() {
        val engine: Engine = mock()

        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")

        val engineSession: EngineSession = mock()
        val browserAction: Action = mock()
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(createTab(id = "popupTab", url = "https://www.mozilla.org")),
                    extensions = mapOf(ext.id to WebExtensionState(ext.id, popupSessionId = "popupTab")),
                ),
            ),
        )

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store, openPopupInTab = true)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Toggling again should select popup tab
        var actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)

        store.waitUntilIdle()
        verify(store, times(1)).dispatch(actionCaptor.capture())
        assertEquals("popupTab", (actionCaptor.value as TabListAction.SelectTabAction).tabId)
    }

    @Test
    fun `reacts to action popup being toggled by closing tab as needed`() {
        val engine: Engine = mock()

        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")

        val engineSession: EngineSession = mock()
        val browserAction: Action = mock()
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(createTab(id = "popupTab", url = "https://www.mozilla.org")),
                    selectedTabId = "popupTab",
                    extensions = mapOf(ext.id to WebExtensionState(ext.id, popupSessionId = "popupTab")),
                ),
            ),
        )

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store, openPopupInTab = true)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Toggling again should close tab
        var actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)
        store.waitUntilIdle()

        verify(store).dispatch(actionCaptor.capture())
        assertEquals("popupTab", (actionCaptor.value as TabListAction.RemoveTabAction).tabId)
    }

    @Test
    fun `reacts to action popup being toggled by opening a popup`() {
        val engine: Engine = mock()

        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")

        val engineSession: EngineSession = mock()
        val browserAction: Action = mock()
        val store = spy(
            BrowserStore(
                BrowserState(
                    extensions = mapOf(ext.id to WebExtensionState(ext.id)),
                ),
            ),
        )

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()

        WebExtensionSupport.initialize(engine, store)
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        // Toggling should allow state to have popup EngineSession instance
        delegateCaptor.value.onToggleActionPopup(ext, engineSession, browserAction)
        val actionCaptor = argumentCaptor<mozilla.components.browser.state.action.BrowserAction>()
        verify(store).dispatch(actionCaptor.capture())

        val value = actionCaptor.value
        assertNotNull((value as WebExtensionAction.UpdatePopupSessionAction).popupSession)
    }

    @Test
    fun `invokes onUpdatePermissionRequest callback`() {
        var executed = false
        val engine: Engine = mock()
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        val store = spy(
            BrowserStore(
                BrowserState(
                    extensions = mapOf(ext.id to WebExtensionState(ext.id)),
                ),
            ),
        )

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(
            runtime = engine,
            store = store,
            onUpdatePermissionRequest = { _, _, _, _ ->
                executed = true
            },
        )

        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())
        delegateCaptor.value.onUpdatePermissionRequest(mock(), mock(), mock(), mock())
        assertTrue(executed)
    }

    @Test
    fun `invokes onExtensionsLoaded callback`() {
        var executed = false
        val engine: Engine = mock()

        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        whenever(ext.isBuiltIn()).thenReturn(false)

        val builtInExt: WebExtension = mock()
        whenever(builtInExt.id).thenReturn("test2")
        whenever(builtInExt.isBuiltIn()).thenReturn(true)

        val store = spy(BrowserStore(BrowserState(extensions = mapOf(ext.id to WebExtensionState(ext.id)))))

        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(listOf(ext, builtInExt))
        }

        val onExtensionsLoaded: ((List<WebExtension>) -> Unit) = {
            assertEquals(1, it.size)
            assertEquals(ext, it[0])
            executed = true
        }
        WebExtensionSupport.initialize(runtime = engine, store = store, onExtensionsLoaded = onExtensionsLoaded)
        assertTrue(executed)
    }

    @Test
    fun `reacts to extension list being updated in the engine`() {
        val store = spy(BrowserStore())
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("test")
        whenever(ext.isEnabled()).thenReturn(true)
        val installedList = mutableListOf(ext)

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(installedList)
        }

        val delegateCaptor = argumentCaptor<WebExtensionDelegate>()
        WebExtensionSupport.initialize(engine, store)
        assertEquals(1, WebExtensionSupport.installedExtensions.size)
        assertEquals(ext, WebExtensionSupport.installedExtensions[ext.id])
        verify(engine).registerWebExtensionDelegate(delegateCaptor.capture())

        delegateCaptor.value.onExtensionListUpdated()
        store.waitUntilIdle()

        val actionCaptor = argumentCaptor<WebExtensionAction>()
        verify(store, times(3)).dispatch(actionCaptor.capture())
        assertEquals(3, actionCaptor.allValues.size)
        // Initial install
        assertTrue(actionCaptor.allValues[0] is WebExtensionAction.InstallWebExtensionAction)
        assertEquals(WebExtensionState(ext.id), (actionCaptor.allValues[0] as WebExtensionAction.InstallWebExtensionAction).extension)

        // Uninstall all
        assertTrue(actionCaptor.allValues[1] is WebExtensionAction.UninstallAllWebExtensionsAction)

        // Reinstall
        assertTrue(actionCaptor.allValues[2] is WebExtensionAction.InstallWebExtensionAction)
        assertEquals(WebExtensionState(ext.id), (actionCaptor.allValues[2] as WebExtensionAction.InstallWebExtensionAction).extension)
        assertEquals(ext, WebExtensionSupport.installedExtensions[ext.id])

        // Verify installed extensions are cleared
        installedList.clear()
        delegateCaptor.value.onExtensionListUpdated()
        store.waitUntilIdle()
        assertTrue(WebExtensionSupport.installedExtensions.isEmpty())
    }

    @Test
    fun `closes tabs from unsupported extensions`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "1", url = "https://www.mozilla.org", restored = true),
                    createTab(id = "2", url = "moz-extension://1234-5678/test", restored = true),
                    createTab(id = "3", url = "moz-extension://1234-5678-9/", restored = true),
                ),
            ),
        )

        val ext1: WebExtension = mock()
        val ext1Meta: Metadata = mock()
        whenever(ext1Meta.baseUrl).thenReturn("moz-extension://1234-5678/")
        whenever(ext1.id).thenReturn("1")
        whenever(ext1.url).thenReturn("url1")
        whenever(ext1.getMetadata()).thenReturn(ext1Meta)
        whenever(ext1.isEnabled()).thenReturn(true)
        whenever(ext1.isAllowedInPrivateBrowsing()).thenReturn(true)

        val ext2: WebExtension = mock()
        whenever(ext2.id).thenReturn("2")
        whenever(ext2.url).thenReturn("url2")
        whenever(ext2.isEnabled()).thenReturn(true)
        whenever(ext2.isAllowedInPrivateBrowsing()).thenReturn(false)

        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(listOf(ext1, ext2))
        }

        WebExtensionSupport.initialize(engine, store)

        store.waitUntilIdle()
        assertNotNull(store.state.findTab("1"))
        assertNotNull(store.state.findTab("2"))
        assertNull(store.state.findTab("3"))

        // Make sure we're running a single cleanup and stop the scope after
        store.dispatch(TabListAction.AddTabAction(createTab(id = "4", url = "moz-extension://1234-5678-90/")))
            .joinBlocking()

        store.waitUntilIdle()
        assertNotNull(store.state.findTab("4"))
    }

    @Test
    fun `marks extensions as updated`() {
        val engineSession: EngineSession = mock()
        val tab =
            createTab(id = "1", url = "https://www.mozilla.org", engineSession = engineSession)

        val customTabEngineSession: EngineSession = mock()
        val customTab =
            createCustomTab(id = "2", url = "https://www.mozilla.org", engineSession = customTabEngineSession, source = SessionState.Source.Internal.CustomTab)

        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(tab),
                    customTabs = listOf(customTab),
                ),
            ),
        )

        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("extensionId")
        whenever(ext.url).thenReturn("url")
        whenever(ext.supportActions).thenReturn(true)

        WebExtensionSupport.markExtensionAsUpdated(store, ext)
        assertSame(ext, WebExtensionSupport.installedExtensions[ext.id])
        verify(store).dispatch(WebExtensionAction.UpdateWebExtensionAction(ext.toState()))

        // Verify that we register new action and tab handlers for the updated extension
        val actionHandlerCaptor = argumentCaptor<ActionHandler>()
        val tabHandlerCaptor = argumentCaptor<TabHandler>()
        verify(ext).registerActionHandler(eq(customTabEngineSession), actionHandlerCaptor.capture())
        verify(ext).registerTabHandler(eq(customTabEngineSession), tabHandlerCaptor.capture())
        verify(ext).registerActionHandler(eq(engineSession), actionHandlerCaptor.capture())
        verify(ext).registerTabHandler(eq(engineSession), tabHandlerCaptor.capture())
    }
}
