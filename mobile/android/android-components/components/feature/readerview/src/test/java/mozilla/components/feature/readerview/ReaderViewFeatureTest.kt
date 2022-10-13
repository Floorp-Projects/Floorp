/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview

import android.content.Context
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.ReaderAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.FONT_SIZE_DEFAULT
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.READER_VIEW_ACTIVE_CONTENT_PORT
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.READER_VIEW_CONTENT_PORT
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.READER_VIEW_EXTENSION_ID
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.READER_VIEW_EXTENSION_URL
import mozilla.components.feature.readerview.view.ReaderViewControlsBar
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import mozilla.components.support.webextensions.WebExtensionController
import mozilla.ext.appCompatContext
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class ReaderViewFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        WebExtensionController.installedExtensions.clear()
    }

    @Test
    fun `start installs webextension`() {
        val engine: Engine = mock()
        val store = BrowserStore()
        val readerViewFeature = ReaderViewFeature(testContext, engine, store, mock())

        readerViewFeature.start()

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_ID),
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_URL),
            onSuccess.capture(),
            onError.capture(),
        )

        onSuccess.value.invoke(mock())

        // Already installed, should not try to install again.
        readerViewFeature.start()
        verify(engine, times(1)).installWebExtension(
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_ID),
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_URL),
            any(),
            any(),
        )
    }

    @Test
    fun `start registers content message handlers for selected session`() {
        val engine: Engine = mock()
        val view: ReaderViewControlsView = mock()
        val engineSession: EngineSession = mock()
        val controller = spy(
            WebExtensionController(
                READER_VIEW_EXTENSION_ID,
                READER_VIEW_EXTENSION_URL,
                READER_VIEW_CONTENT_PORT,
            ),
        )
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "test-tab",
            engineSession = engineSession,
        )
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            ),
        )

        val readerViewFeature = ReaderViewFeature(testContext, engine, store, view)
        readerViewFeature.extensionController = controller
        readerViewFeature.start()

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_ID),
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_URL),
            onSuccess.capture(),
            onError.capture(),
        )
        onSuccess.value.invoke(mock())
        verify(controller).registerContentMessageHandler(eq(engineSession), any(), eq(READER_VIEW_ACTIVE_CONTENT_PORT))
        verify(controller).registerContentMessageHandler(eq(engineSession), any(), eq(READER_VIEW_CONTENT_PORT))
    }

    @Test
    fun `start also starts controls interactor`() {
        val engine: Engine = mock()
        val store = BrowserStore()
        val view: ReaderViewControlsView = ReaderViewControlsBar(appCompatContext)

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, view))
        readerViewFeature.start()

        assertNotNull(view.listener)
    }

    @Test
    fun `stop also stops controls interactor`() {
        val engine: Engine = mock()
        val store = BrowserStore()
        val view: ReaderViewControlsView = ReaderViewControlsBar(appCompatContext)

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, view))
        readerViewFeature.stop()

        assertNull(view.listener)
    }

    @Test
    fun `showControls invokes the controls presenter`() {
        val view: ReaderViewControlsView = mock()
        val feature = spy(ReaderViewFeature(testContext, mock(), mock(), view))

        feature.showControls()

        verify(view).setColorScheme(any())
        verify(view).setFont(any())
        verify(view).setFontSize(anyInt())
        verify(view).showControls()
    }

    @Test
    fun `hideControls invokes the controls presenter`() {
        val view: ReaderViewControlsView = mock()
        val feature = spy(ReaderViewFeature(testContext, mock(), mock(), view))

        feature.hideControls()

        verify(view).hideControls()
    }

    @Test
    fun `triggers readerable check when required`() {
        val engine: Engine = mock()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(initialState = BrowserState(tabs = listOf(tab)))
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, mock()))
        readerViewFeature.start()

        store.dispatch(ReaderAction.UpdateReaderableCheckRequiredAction(tab.id, true)).joinBlocking()

        val tabCaptor = argumentCaptor<TabSessionState>()
        verify(readerViewFeature).checkReaderState(tabCaptor.capture())
        assertEquals(tab.id, tabCaptor.value.id)
    }

    @Test
    fun `connects content script port when required`() {
        val engine: Engine = mock()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(initialState = BrowserState(tabs = listOf(tab), selectedTabId = tab.id))
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, mock()))
        readerViewFeature.start()

        store.dispatch(ReaderAction.UpdateReaderConnectRequiredAction(tab.id, true)).joinBlocking()
        val tabCaptor = argumentCaptor<TabSessionState>()
        verify(readerViewFeature).connectReaderViewContentScript(tabCaptor.capture())
        assertEquals(tab.id, tabCaptor.value.id)
    }

    @Test
    fun `notifies readerable state changes of selected tab`() {
        val readerViewStatusChanges = mutableListOf<Pair<Boolean, Boolean>>()
        val onReaderViewStatusChange: onReaderViewStatusChange = {
                readerable, active ->
            readerViewStatusChanges.add(Pair(readerable, active))
        }

        val engine: Engine = mock()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(initialState = BrowserState(tabs = listOf(tab)))
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, mock(), onReaderViewStatusChange))
        readerViewFeature.start()
        assertTrue(readerViewStatusChanges.isEmpty())

        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()
        store.dispatch(ReaderAction.UpdateReaderableAction(tab.id, true)).joinBlocking()
        assertEquals(1, readerViewStatusChanges.size)
        assertEquals(Pair(true, false), readerViewStatusChanges[0])

        store.dispatch(ReaderAction.UpdateReaderActiveAction(tab.id, true)).joinBlocking()
        assertEquals(2, readerViewStatusChanges.size)
        assertEquals(Pair(true, true), readerViewStatusChanges[1])

        store.dispatch(ReaderAction.UpdateReaderableAction(tab.id, true)).joinBlocking()
        // No change -> No notification should have been sent
        assertEquals(2, readerViewStatusChanges.size)

        store.dispatch(ReaderAction.UpdateReaderActiveAction(tab.id, false)).joinBlocking()
        assertEquals(3, readerViewStatusChanges.size)
        assertEquals(Pair(true, false), readerViewStatusChanges[2])

        store.dispatch(ReaderAction.UpdateReaderableAction(tab.id, false)).joinBlocking()
        assertEquals(4, readerViewStatusChanges.size)
        assertEquals(Pair(false, false), readerViewStatusChanges[3])
    }

    @Test
    fun `show reader view sends message to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()
        val readerViewFeature = prepareFeatureForTest(port)

        readerViewFeature.showReaderView()
        verify(port).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_SHOW, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    @Test
    fun `default values used for showing reader view if no config is present`() {
        val message = ReaderViewFeature.createShowReaderMessage(null)
        assertEquals(ReaderViewFeature.ACTION_SHOW, message[ReaderViewFeature.ACTION_MESSAGE_KEY])
        val config = message[ReaderViewFeature.ACTION_VALUE] as JSONObject?
        assertNotNull(config)
        assertEquals(FONT_SIZE_DEFAULT, config!![ReaderViewFeature.ACTION_VALUE_SHOW_FONT_SIZE])
        assertEquals(
            ReaderViewFeature.FontType.SERIF.value.lowercase(Locale.ROOT),
            config[ReaderViewFeature.ACTION_VALUE_SHOW_FONT_TYPE],
        )
        assertEquals(
            ReaderViewFeature.ColorScheme.LIGHT.name.lowercase(Locale.ROOT),
            config[ReaderViewFeature.ACTION_VALUE_SHOW_COLOR_SCHEME],
        )
    }

    @Test
    fun `show reader view updates state`() {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "test-tab",
            engineSession = engineSession,
        )
        val store = spy(
            BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tab.id,
                ),
            ),
        )
        val readerViewFeature = ReaderViewFeature(testContext, engine, store, mock())
        readerViewFeature.showReaderView()
        verify(store).dispatch(ReaderAction.UpdateReaderActiveAction(tab.id, true))
    }

    @Test
    fun `hide reader view navigates back if possible`() {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        val tab = createTab("https://www.mozilla.org", id = "test-tab", readerState = ReaderState(active = true))
        val store = BrowserStore(initialState = BrowserState(tabs = listOf(tab)))
        val readerViewFeature = ReaderViewFeature(testContext, engine, store, mock())
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()
        store.dispatch(ContentAction.UpdateBackNavigationStateAction(tab.id, true)).joinBlocking()

        readerViewFeature.hideReaderView()
        verify(engineSession).goBack(false)
    }

    @Test
    fun `hide reader view sends message to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()
        val readerViewFeature = prepareFeatureForTest(
            readerActivePort = port,
            tab = createTab("https://www.mozilla.org", id = "test-tab", readerState = ReaderState(active = true)),
        )

        readerViewFeature.hideReaderView()
        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_HIDE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    @Test
    fun `hide reader view updates state`() {
        val engine: Engine = mock()
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "test-tab",
            readerState = ReaderState(active = true),
        )

        val store = spy(
            BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tab.id,
                ),
            ),
        )
        val readerViewFeature = ReaderViewFeature(testContext, engine, store, mock())
        readerViewFeature.hideReaderView()
        verify(store).dispatch(ReaderAction.UpdateReaderActiveAction(tab.id, false))
        verify(store).dispatch(ReaderAction.UpdateReaderableAction(tab.id, false))
        verify(store).dispatch(ReaderAction.ClearReaderActiveUrlAction(tab.id))
    }

    @Test
    fun `reader state check sends message to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()
        val readerViewFeature = prepareFeatureForTest(port)

        readerViewFeature.checkReaderState()
        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHECK_READER_STATE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    @Test
    fun `color scheme config change persists and is sent to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()

        val readerViewFeature = prepareFeatureForTest(readerActivePort = port)
        val prefs = testContext.getSharedPreferences(ReaderViewFeature.SHARED_PREF_NAME, Context.MODE_PRIVATE)

        readerViewFeature.config.colorScheme = ReaderViewFeature.ColorScheme.DARK
        assertEquals(ReaderViewFeature.ColorScheme.DARK.name, prefs.getString(ReaderViewFeature.COLOR_SCHEME_KEY, null))

        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_SET_COLOR_SCHEME, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
        assertEquals(ReaderViewFeature.ColorScheme.DARK.name, message.value[ReaderViewFeature.ACTION_VALUE])

        // Setting to the same value should not cause another message to be sent
        readerViewFeature.config.colorScheme = ReaderViewFeature.ColorScheme.DARK
        verify(port, times(1)).postMessage(message.capture())
    }

    @Test
    fun `font type config change persists and is sent to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()

        val readerViewFeature = prepareFeatureForTest(readerActivePort = port)
        val prefs = testContext.getSharedPreferences(ReaderViewFeature.SHARED_PREF_NAME, Context.MODE_PRIVATE)

        readerViewFeature.config.fontType = ReaderViewFeature.FontType.SANSSERIF
        assertEquals(ReaderViewFeature.FontType.SANSSERIF.name, prefs.getString(ReaderViewFeature.FONT_TYPE_KEY, null))

        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_SET_FONT_TYPE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
        assertEquals(ReaderViewFeature.FontType.SANSSERIF.value, message.value[ReaderViewFeature.ACTION_VALUE])

        // Setting to the same value should not cause another message to be sent
        readerViewFeature.config.fontType = ReaderViewFeature.FontType.SANSSERIF
        verify(port, times(1)).postMessage(message.capture())
    }

    @Test
    fun `font size config change persists and is sent to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()

        val readerViewFeature = prepareFeatureForTest(readerActivePort = port)
        val prefs = testContext.getSharedPreferences(ReaderViewFeature.SHARED_PREF_NAME, Context.MODE_PRIVATE)

        readerViewFeature.config.fontSize = 4
        assertEquals(4, prefs.getInt(ReaderViewFeature.FONT_SIZE_KEY, 0))

        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHANGE_FONT_SIZE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
        assertEquals(1, message.value[ReaderViewFeature.ACTION_VALUE])

        // Setting to the same value should not cause another message to be sent
        readerViewFeature.config.fontSize = 4
        verify(port, times(1)).postMessage(message.capture())
    }

    @Test
    fun `on back pressed hides controls`() {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(BrowserState(tabs = listOf(tab)))
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()

        val controlsView: ReaderViewControlsView = mock()
        val view: View = mock()
        whenever(controlsView.asView()).thenReturn(view)

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, controlsView))
        assertFalse(readerViewFeature.onBackPressed())

        store.dispatch(ReaderAction.UpdateReaderActiveAction(tab.id, true)).joinBlocking()
        whenever(view.visibility).thenReturn(View.VISIBLE)
        assertTrue(readerViewFeature.onBackPressed())
        verify(readerViewFeature, never()).hideReaderView(any())
        verify(readerViewFeature, times(1)).hideControls()

        whenever(view.visibility).thenReturn(View.GONE)
        assertTrue(readerViewFeature.onBackPressed())
        verify(readerViewFeature, times(1)).hideReaderView(any())
        verify(readerViewFeature, times(1)).hideControls()
    }

    @Test
    fun `state is updated when reader state arrives`() {
        val engine: Engine = mock()
        val view: ReaderViewControlsView = mock()
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val controller: WebExtensionController = mock()
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "test-tab",
            engineSession = engineSession,
        )
        val store = spy(
            BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tab.id,
                ),
            ),
        )

        WebExtensionController.installedExtensions[ReaderViewFeature.READER_VIEW_EXTENSION_ID] = ext

        val port: Port = mock()
        whenever(port.engineSession).thenReturn(engineSession)
        whenever(ext.getConnectedPort(any(), any())).thenReturn(port)

        whenever(controller.portConnected(any(), any())).thenReturn(true)
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, view))
        readerViewFeature.extensionController = controller

        val messageHandler = argumentCaptor<MessageHandler>()
        val message = argumentCaptor<JSONObject>()
        readerViewFeature.start()
        store.dispatch(ReaderAction.UpdateReaderConnectRequiredAction(tab.id, true)).joinBlocking()
        verify(controller).registerContentMessageHandler(
            eq(engineSession),
            messageHandler.capture(),
            eq(READER_VIEW_ACTIVE_CONTENT_PORT),
        )

        messageHandler.value.onPortConnected(port)
        verify(port).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHECK_READER_STATE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])

        val readerStateMessage = JSONObject()
            .put("readerable", true)
            .put("baseUrl", "moz-extension://")
            .put("activeUrl", "http://mozilla.org/article")
        messageHandler.value.onPortMessage(readerStateMessage, port)
        verify(store).dispatch(ReaderAction.UpdateReaderableAction(tab.id, true))
        verify(store).dispatch(ReaderAction.UpdateReaderBaseUrlAction(tab.id, "moz-extension://"))
        verify(store).dispatch(ReaderAction.UpdateReaderActiveUrlAction(tab.id, "http://mozilla.org/article"))
    }

    @Test
    fun `reader is shown when state arrives from reader page`() {
        val engine: Engine = mock()
        val view: ReaderViewControlsView = mock()
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val controller: WebExtensionController = mock()
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "test-tab",
            engineSession = engineSession,
        )
        val store = spy(
            BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(tab),
                    selectedTabId = tab.id,
                ),
            ),
        )

        WebExtensionController.installedExtensions[READER_VIEW_EXTENSION_ID] = ext

        val port: Port = mock()
        whenever(port.engineSession).thenReturn(engineSession)
        whenever(ext.getConnectedPort(any(), any())).thenReturn(port)

        whenever(controller.portConnected(any(), any())).thenReturn(true)
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, store, view))
        readerViewFeature.extensionController = controller

        val messageHandler = argumentCaptor<MessageHandler>()
        val message = argumentCaptor<JSONObject>()
        readerViewFeature.start()

        store.dispatch(ReaderAction.UpdateReaderConnectRequiredAction(tab.id, true)).joinBlocking()
        verify(controller).registerContentMessageHandler(
            eq(engineSession),
            messageHandler.capture(),
            eq(READER_VIEW_ACTIVE_CONTENT_PORT),
        )
        messageHandler.value.onPortConnected(port)

        val readerStateMessage = JSONObject()
            .put("readerable", true)
            .put("baseUrl", "moz-extension://")
            .put("activeUrl", "http://mozilla.org/article")
        messageHandler.value.onPortMessage(readerStateMessage, port)
        verify(port, times(2)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHECK_READER_STATE, message.allValues[0][ReaderViewFeature.ACTION_MESSAGE_KEY])
        assertEquals(ReaderViewFeature.ACTION_SHOW, message.allValues[1][ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    private fun prepareFeatureForTest(
        contentPort: Port? = null,
        readerActivePort: Port? = null,
        tab: TabSessionState = createTab("https://www.mozilla.org", id = "test-tab"),
        engineSession: EngineSession = mock(),
        controller: WebExtensionController? = null,
    ): ReaderViewFeature {
        val engine: Engine = mock()

        val store = BrowserStore(BrowserState(tabs = listOf(tab)))
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()

        val ext: WebExtension = mock()
        contentPort?.let {
            whenever(ext.getConnectedPort(eq(ReaderViewFeature.READER_VIEW_CONTENT_PORT), any()))
                .thenReturn(contentPort)
        }
        readerActivePort?.let {
            whenever(ext.getConnectedPort(eq(ReaderViewFeature.READER_VIEW_ACTIVE_CONTENT_PORT), any()))
                .thenReturn(readerActivePort)
        }
        WebExtensionController.installedExtensions[ReaderViewFeature.READER_VIEW_EXTENSION_ID] = ext

        val feature = ReaderViewFeature(testContext, engine, store, mock())
        if (controller != null) {
            feature.extensionController = controller
        }
        return feature
    }
}
