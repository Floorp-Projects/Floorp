/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview

import android.content.Context
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.readerview.view.ReaderViewControlsBar
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
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
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ReaderViewFeatureTest {

    @Before
    fun setup() {
        WebExtensionController.installedExtensions.clear()
    }

    @Test
    fun `start installs webextension`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val readerViewFeature = ReaderViewFeature(testContext, engine, sessionManager, mock())

        readerViewFeature.start()

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_ID),
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_URL),
            eq(true),
            onSuccess.capture(),
            onError.capture()
        )

        onSuccess.value.invoke(mock())

        // Already installed, should not try to install again.
        readerViewFeature.start()
        verify(engine, times(1)).installWebExtension(
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_ID),
            eq(ReaderViewFeature.READER_VIEW_EXTENSION_URL),
            eq(true),
            any(),
            any()
        )
    }

    @Test
    fun `start registers observer for selected session`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))
        readerViewFeature.start()

        verify(readerViewFeature).observeSelected()
    }

    @Test
    fun `start registers content message handler for selected session`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val message = argumentCaptor<JSONObject>()
        val controller = mock<WebExtensionController>()

        WebExtensionController.installedExtensions[ReaderViewFeature.READER_VIEW_EXTENSION_ID] = ext

        val port = mock<Port>()
        whenever(port.engineSession).thenReturn(engineSession)
        whenever(ext.getConnectedPort(any(), any())).thenReturn(port)

        whenever(controller.portConnected(any(), any())).thenReturn(true)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(sessionManager.selectedSession).thenReturn(session)
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))
        readerViewFeature.extensionController = controller

        readerViewFeature.start()
        verify(controller).registerContentMessageHandler(eq(engineSession), messageHandler.capture(), any())

        messageHandler.value.onPortConnected(port)
        verify(port).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHECK_READERABLE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])

        val readerableMessage = JSONObject().put("readerable", true)
        messageHandler.value.onPortMessage(readerableMessage, port)
        verify(session).readerable = true
    }

    @Test
    fun `port is removed with session`() {
        val port = mock<Port>()
        val controller = mock<WebExtensionController>()
        val selectedSession = mock<Session>()
        val selectedEngineSession = mock<EngineSession>()
        val readerViewFeature = prepareFeatureForTest(port, selectedSession, selectedEngineSession, controller)

        readerViewFeature.onSessionRemoved(selectedSession)
        verify(controller).disconnectPort(eq(selectedEngineSession), any())
    }

    @Test
    fun `start also starts controls interactor`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view: ReaderViewControlsView = ReaderViewControlsBar(appCompatContext)

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))
        readerViewFeature.start()

        assertNotNull(view.listener)
    }

    @Test
    fun `stop also stops controls interactor`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view: ReaderViewControlsView = ReaderViewControlsBar(appCompatContext)

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))
        readerViewFeature.stop()

        assertNull(view.listener)
    }

    @Test
    fun `showControls invokes the controls presenter`() {
        val view = mock<ReaderViewControlsView>()
        val feature = spy(ReaderViewFeature(testContext, mock(), mock(), view))

        feature.showControls()

        verify(view).setColorScheme(any())
        verify(view).setFont(any())
        verify(view).setFontSize(anyInt())
        verify(view).showControls()
    }

    @Test
    fun `hideControls invokes the controls presenter`() {
        val view = mock<ReaderViewControlsView>()
        val feature = spy(ReaderViewFeature(testContext, mock(), mock(), view))

        feature.hideControls()

        verify(view).hideControls()
    }

    @Test
    fun `check readerable on start`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()
        val controller = mock<WebExtensionController>()
        val selectedSession = mock<Session>()

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))
        readerViewFeature.extensionController = controller
        readerViewFeature.start()
        verify(readerViewFeature, never()).updateReaderViewState(any())

        whenever(controller.portConnected(any(), any())).thenReturn(true)
        readerViewFeature.start()
        verify(readerViewFeature, never()).updateReaderViewState(any())

        readerViewFeature.onSessionSelected(selectedSession)
        readerViewFeature.start()
        verify(readerViewFeature).updateReaderViewState(selectedSession)
    }

    @Test
    fun `update reader view state when session is selected`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()
        val selectedSession = mock<Session>()

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))
        readerViewFeature.onSessionSelected(selectedSession)

        verify(readerViewFeature).updateReaderViewState(eq(selectedSession))
    }

    @Test
    fun `register content message handler for added session`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val controller = mock<WebExtensionController>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val message = argumentCaptor<JSONObject>()

        WebExtensionController.installedExtensions[ReaderViewFeature.READER_VIEW_EXTENSION_ID] = ext

        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))
        readerViewFeature.extensionController = controller

        readerViewFeature.onSessionAdded(session)
        verify(controller).registerContentMessageHandler(eq(engineSession), messageHandler.capture(), any())

        val port = mock<Port>()
        whenever(port.engineSession).thenReturn(engineSession)

        messageHandler.value.onPortConnected(port)
        verify(port).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHECK_READERABLE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    @Test
    fun `check readerable when url changed`() {
        val engine = mock<Engine>()
        val session = mock<Session>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))

        readerViewFeature.onUrlChanged(session, "")
        verify(readerViewFeature).checkReaderable(session)
    }

    @Test
    fun `show reader view sends message to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()
        val readerViewFeature = prepareFeatureForTest(port)

        readerViewFeature.showReaderView()
        verify(port, never()).postMessage(any())

        readerViewFeature.observeSelected()
        readerViewFeature.showReaderView()
        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_SHOW, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    @Test
    fun `hide reader view sends message to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()
        val readerViewFeature = prepareFeatureForTest(port)

        readerViewFeature.hideReaderView()
        verify(port, never()).postMessage(any())

        readerViewFeature.observeSelected()
        readerViewFeature.hideReaderView()
        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_HIDE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    @Test
    fun `readerable check sends message to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()
        val readerViewFeature = prepareFeatureForTest(port)

        readerViewFeature.checkReaderable()
        verify(port, never()).postMessage(any())

        readerViewFeature.observeSelected()
        readerViewFeature.checkReaderable()
        verify(port, times(1)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHECK_READERABLE, message.value[ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    @Test
    fun `readerable state update invokes lambda`() {
        var readerViewAvailableChangeReceived = false
        val onReaderViewAvailableChange: OnReaderViewAvailableChange = { readerViewAvailableChangeReceived = true }
        val readerViewFeature = spy(ReaderViewFeature(testContext, mock(), mock(), mock(), onReaderViewAvailableChange))
        readerViewFeature.onReaderableStateUpdated(mock(), true)
        assertTrue(readerViewAvailableChangeReceived)
    }

    @Test
    fun `color scheme config change persists and is sent to web extension`() {
        val port = mock<Port>()
        val message = argumentCaptor<JSONObject>()

        val readerViewFeature = prepareFeatureForTest(port)
        readerViewFeature.observeSelected()

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

        val readerViewFeature = prepareFeatureForTest(port)
        readerViewFeature.observeSelected()

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

        val readerViewFeature = prepareFeatureForTest(port)
        readerViewFeature.observeSelected()

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
    fun `update state checks readerable and shows reader mode`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()
        val session = mock<Session>()

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))

        readerViewFeature.updateReaderViewState(null)
        verify(readerViewFeature, never()).checkReaderable(any())
        verify(readerViewFeature, never()).showReaderView(any())

        readerViewFeature.updateReaderViewState(session)
        verify(readerViewFeature).checkReaderable(eq(session))
        verify(readerViewFeature, never()).showReaderView(any())

        whenever(session.readerMode).thenReturn(true)
        readerViewFeature.updateReaderViewState(session)
        verify(readerViewFeature, times(2)).checkReaderable(eq(session))
        verify(readerViewFeature).showReaderView(eq(session))

        val selectedSession = mock<Session>()
        whenever(selectedSession.readerMode).thenReturn(true)
        whenever(sessionManager.selectedSession).thenReturn(selectedSession)
        readerViewFeature.observeSelected()
        readerViewFeature.updateReaderViewState(selectedSession)
        verify(readerViewFeature).checkReaderable(eq(selectedSession))
        verify(readerViewFeature).showReaderView(eq(selectedSession))
    }

    @Test
    fun `on back pressed closes controls then reader view`() {
        val engine = mock<Engine>()
        val session = mock<Session>()
        val sessionManager = mock<SessionManager>()
        whenever(sessionManager.selectedSession).thenReturn(session)

        val controlsView = mock<ReaderViewControlsView>()
        val view = mock<View>()
        whenever(controlsView.asView()).thenReturn(view)

        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, controlsView))
        assertFalse(readerViewFeature.onBackPressed())

        readerViewFeature.observeSelected()
        assertFalse(readerViewFeature.onBackPressed())

        whenever(session.readerMode).thenReturn(true)
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
    fun `reader mode is activated when port connects`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val view = mock<ReaderViewControlsView>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val message = argumentCaptor<JSONObject>()

        WebExtensionController.installedExtensions[ReaderViewFeature.READER_VIEW_EXTENSION_ID] = ext

        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(sessionManager.selectedSession).thenReturn(session)
        val readerViewFeature = spy(ReaderViewFeature(testContext, engine, sessionManager, view))

        readerViewFeature.start()
        verify(ext).registerContentMessageHandler(eq(engineSession), eq(ReaderViewFeature.READER_VIEW_EXTENSION_ID), messageHandler.capture())

        val port = mock<Port>()
        whenever(port.engineSession).thenReturn(engineSession)

        `when`(session.readerMode).thenReturn(true)
        messageHandler.value.onPortConnected(port)
        verify(port, times(2)).postMessage(message.capture())
        assertEquals(ReaderViewFeature.ACTION_CHECK_READERABLE, message.allValues[0][ReaderViewFeature.ACTION_MESSAGE_KEY])
        assertEquals(ReaderViewFeature.ACTION_SHOW, message.allValues[1][ReaderViewFeature.ACTION_MESSAGE_KEY])
    }

    private fun prepareFeatureForTest(
        port: Port,
        session: Session = mock(),
        engineSession: EngineSession = mock(),
        controller: WebExtensionController? = null
    ): ReaderViewFeature {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val ext = mock<WebExtension>()

        whenever(ext.getConnectedPort(eq(ReaderViewFeature.READER_VIEW_EXTENSION_ID), any())).thenReturn(port)
        whenever(sessionManager.selectedSession).thenReturn(session)
        whenever(sessionManager.getEngineSession(session)).thenReturn(engineSession)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        WebExtensionController.installedExtensions[ReaderViewFeature.READER_VIEW_EXTENSION_ID] = ext

        val feature = ReaderViewFeature(testContext, engine, sessionManager, mock())
        if (controller != null) {
            feature.extensionController = controller
        }
        return feature
    }
}
