/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.sync.AuthType
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class FxaWebChannelFeatureTest {

    @Before
    fun setup() {
        FxaWebChannelFeature.installedWebExt = null
    }

    @Test
    fun `start installs webextension`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()

        val webchannelFeature = FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager)
        assertNull(FxaWebChannelFeature.installedWebExt)
        webchannelFeature.start()

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
                eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
                eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_URL),
                eq(true),
                onSuccess.capture(),
                onError.capture()
        )

        onSuccess.value.invoke(mock())
        assertNotNull(FxaWebChannelFeature.installedWebExt)

        webchannelFeature.start()
        verify(engine, times(1)).installWebExtension(
                eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
                eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_URL),
                eq(true),
                onSuccess.capture(),
                onError.capture()
        )

        onError.value.invoke(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID, RuntimeException())
    }
    @Test
    fun `start registers observer for selected session`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()

        val webchannelFeature = spy(FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager))
        webchannelFeature.start()

        verify(webchannelFeature).observeSelected()
    }

    @Test
    fun `start registers content message handler for selected session`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()

        FxaWebChannelFeature.installedWebExt = ext

        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(sessionManager.selectedSession).thenReturn(session)
        val webchannelFeature = spy(FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager))

        webchannelFeature.start()
        verify(ext).registerContentMessageHandler(eq(engineSession), eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID), messageHandler.capture())

        val port = mock<Port>()
        whenever(port.engineSession).thenReturn(engineSession)

        messageHandler.value.onPortConnected(port)
        assertTrue(FxaWebChannelFeature.ports.containsValue(port))

        messageHandler.value.onPortDisconnected(port)
        assertFalse(FxaWebChannelFeature.ports.containsValue(port))
    }

    @Test
    fun `port is removed with session`() {
        val port = mock<Port>()
        val selectedSession = mock<Session>()
        val webchannelFeature = prepareFeatureForTest(port, selectedSession)

        val size = FxaWebChannelFeature.ports.size
        webchannelFeature.onSessionRemoved(selectedSession)
        assertEquals(size - 1, FxaWebChannelFeature.ports.size)
    }

    @Test
    fun `register content message handler for added session`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()

        FxaWebChannelFeature.installedWebExt = ext

        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        val webchannelFeature = spy(FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(eq(engineSession), eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID), messageHandler.capture())

        val port = mock<Port>()
        whenever(port.engineSession).thenReturn(engineSession)

        messageHandler.value.onPortConnected(port)
        assertTrue(FxaWebChannelFeature.ports.containsValue(port))
    }

    // Receiving and responding a fxa-status message if sync is configured with one engine
    @Test
    fun `COMMAND_STATUS configured with one engine must be provided to the web-channel`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.HISTORY)

        FxaWebChannelFeature.installedWebExt = ext

        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }""".trimIndent()
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, mock())
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getCapabilities()
        assertTrue(capabilitiesFromWebChannel.size == 1)

        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    // Receiving and responding a fxa-status message if sync is configured with more than one engine
    @Test
    fun `COMMAND_STATUS configured with more than one engine must be provided to the web-channel`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.HISTORY, SyncEngine.BOOKMARKS, SyncEngine.PASSWORDS)

        FxaWebChannelFeature.installedWebExt = ext

        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }""".trimIndent()
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, mock())
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getCapabilities()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    // Receiving and responding a fxa-status message if account manager is logged in
    @Test
    fun `COMMAND_STATUS with account manager is logged in`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.HISTORY, SyncEngine.BOOKMARKS, SyncEngine.PASSWORDS)
        val logoutDeferred = CompletableDeferred<Unit>()

        FxaWebChannelFeature.installedWebExt = ext

        whenever(accountManager.accountProfile()).thenReturn(mock())
        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(accountManager.logoutAsync()).thenReturn(logoutDeferred)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }""".trimIndent()
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, mock())
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getCapabilities()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    // Receiving and responding a fxa-status message if account manager is logged out
    @Test
    fun `COMMAND_STATUS with account manager is logged out`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.HISTORY, SyncEngine.BOOKMARKS, SyncEngine.PASSWORDS)

        FxaWebChannelFeature.installedWebExt = ext

        whenever(accountManager.accountProfile()).thenReturn(null)
        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }""".trimIndent()
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, mock())
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getCapabilities()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    // Receiving and responding a fxa-status message if account manager when sync is not configured
    @Test
    fun `COMMAND_STATUS with account manager when sync is not configured`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>() // syncConfig is null by default (is not configured)
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.HISTORY, SyncEngine.BOOKMARKS, SyncEngine.PASSWORDS)

        FxaWebChannelFeature.installedWebExt = ext

        whenever(accountManager.accountProfile()).thenReturn(null)
        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }""".trimIndent()
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, mock())
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getCapabilities()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    @Test
    fun `COMMAND_STATUS with no capabilities configured must provided an empty list of engines to the web-channel`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()

        FxaWebChannelFeature.installedWebExt = ext

        whenever(accountManager.supportedSyncEngines()).thenReturn(null)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }""".trimIndent()
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, mock())
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getCapabilities()

        assertTrue(capabilitiesFromWebChannel.isEmpty())
    }

    // Receiving an oauth-login message account manager accepts the request
    @Test
    fun `COMMAND_OAUTH_LOGIN web-channel must be processed through when the accountManager accepts the request`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val port = mock<Port>()

        FxaWebChannelFeature.installedWebExt = ext
        whenever(accountManager.finishAuthenticationAsync(any())).thenReturn(CompletableDeferred(false))
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        // Action: signin
        verifyOauthLogin("signin", AuthType.Signin, "fffs", "fsdf32", messageHandler.value, accountManager)
        // Signup.
        verifyOauthLogin("signup", AuthType.Signup, "anotherCode1", "anotherState2", messageHandler.value, accountManager)
        // Pairing.
        verifyOauthLogin("pairing", AuthType.Pairing, "anotherCode2", "anotherState3", messageHandler.value, accountManager)
        // Some other action.
        verifyOauthLogin("newAction", AuthType.OtherExternal("newAction"), "anotherCode3", "anotherState4", messageHandler.value, accountManager)
    }

    // Receiving an oauth-login message account manager refuses the request
    @Test
    fun `COMMAND_OAUTH_LOGIN web-channel must be processed when the accountManager refuses the request`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val port = mock<Port>()

        FxaWebChannelFeature.installedWebExt = ext
        whenever(accountManager.finishAuthenticationAsync(any())).thenReturn(CompletableDeferred(false))
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        // Action: signin
        verifyOauthLogin("signin", AuthType.Signin, "fffs", "fsdf32", messageHandler.value, accountManager)
        // Signup.
        verifyOauthLogin("signup", AuthType.Signup, "anotherCode1", "anotherState2", messageHandler.value, accountManager)
        // Pairing.
        verifyOauthLogin("pairing", AuthType.Pairing, "anotherCode2", "anotherState3", messageHandler.value, accountManager)
        // Some other action.
        verifyOauthLogin("newAction", AuthType.OtherExternal("newAction"), "anotherCode3", "anotherState4", messageHandler.value, accountManager)
    }

    // Receiving can-link-account  returns 'ok=true' message (for now)
    @Test
    fun `COMMAND_CAN_LINK_ACCOUNT must provide an OK response to the web-channel`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val jsonFromWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.HISTORY, SyncEngine.BOOKMARKS)

        FxaWebChannelFeature.installedWebExt = ext

        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager))

        webchannelFeature.onSessionAdded(session)
        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            messageHandler.capture()
        )
        messageHandler.value.onPortConnected(port)

        val jsonToWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:can_link_account",
                "messageId":123
             }
            }""".trimIndent()
        )

        messageHandler.value.onPortMessage(jsonToWebChannel, mock())
        verify(port).postMessage(jsonFromWebChannel.capture())

        assertTrue(jsonFromWebChannel.value.getOk())
    }

    private fun prepareFeatureForTest(port: Port, session: Session = mock()): FxaWebChannelFeature {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val ext = mock<WebExtension>()
        val engineSession = mock<EngineSession>()

        whenever(sessionManager.selectedSession).thenReturn(session)
        whenever(sessionManager.getEngineSession(session)).thenReturn(engineSession)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        val webchannelFeature = FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager)
        FxaWebChannelFeature.installedWebExt = ext
        FxaWebChannelFeature.ports[engineSession] = port
        return webchannelFeature
    }

    private fun JSONObject.getCapabilities(): List<String> {
        val engines = this.getJSONObject("message")
            .getJSONObject("data")
            .getJSONObject("capabilities")
            .getJSONArray("engines")

        val list = mutableListOf<String>()
        for (i in 0 until engines.length()) {
            list.add(engines[i].toString())
        }
        return list
    }

    private fun JSONObject.isSignedInUserNull(): Boolean {
        return this.getJSONObject("message")
            .getJSONObject("data")
            .isNull("signedInUser")
    }

    private fun JSONObject.getOk(): Boolean {
        return this.getJSONObject("message")
            .getJSONObject("data")
            .getBoolean("ok")
    }

    private fun verifyOauthLogin(action: String, expectedAuthType: AuthType, code: String, state: String, messageHandler: MessageHandler, accountManager: FxaAccountManager) {
        val jsonToWebChannel = jsonOauthLogin(action, code, state)
        messageHandler.onPortMessage(jsonToWebChannel, mock())

        val expectedAuthData = FxaAuthData(
            authType = expectedAuthType,
            code = code,
            state = state
        )
        verify(accountManager).finishAuthenticationAsync(expectedAuthData)
    }

    private fun jsonOauthLogin(action: String, code: String, state: String): JSONObject {
        return JSONObject(
            """{
             "message":{
                "command": "fxaccounts:oauth_login",
                "messageId":123,
                "data":{
                    "action":"$action",
                    "redirect":"urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel",
                    "code":"$code",
                    "state":"$state"
                }
             }
            }""".trimIndent()
        )
    }
}
