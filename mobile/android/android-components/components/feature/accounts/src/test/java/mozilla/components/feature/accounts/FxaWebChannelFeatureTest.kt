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
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import mozilla.components.support.webextensions.WebExtensionController
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
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
        WebExtensionController.installedExtensions.clear()
    }

    @Test
    fun `start installs webextension`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val webchannelFeature = FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager)
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

        // Already installed, should not try to install again.
        webchannelFeature.start()
        verify(engine, times(1)).installWebExtension(
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_URL),
            eq(true),
            any(),
            any()
        )
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
        val controller = mock<WebExtensionController>()

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val port = mock<Port>()
        whenever(port.engineSession).thenReturn(engineSession)
        whenever(ext.getConnectedPort(any(), any())).thenReturn(port)

        whenever(controller.portConnected(any(), any())).thenReturn(true)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(sessionManager.selectedSession).thenReturn(session)
        val webchannelFeature = spy(FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager))
        webchannelFeature.extensionController = controller

        webchannelFeature.start()
        verify(controller).registerContentMessageHandler(eq(engineSession), messageHandler.capture(), any())
    }

    @Test
    fun `port is removed with session`() {
        val port = mock<Port>()
        val controller = mock<WebExtensionController>()
        val selectedSession = mock<Session>()
        val selectedEngineSession = mock<EngineSession>()
        val webchannelFeature = prepareFeatureForTest(port, selectedSession, selectedEngineSession, controller)

        webchannelFeature.onSessionRemoved(selectedSession)
        verify(controller).disconnectPort(eq(selectedEngineSession), any())
    }

    @Test
    fun `register content message handler for added session`() {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val controller = mock<WebExtensionController>()
        val messageHandler = argumentCaptor<MessageHandler>()

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        val webchannelFeature = spy(FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager))
        webchannelFeature.extensionController = controller

        webchannelFeature.onSessionAdded(session)
        verify(controller).registerContentMessageHandler(eq(engineSession), messageHandler.capture(), any())
    }

    @Test
    fun `COMMAND_STATUS configured with CWTS must provided a boolean=true flag to the web-channel`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.History)

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)
        whenever(port.engineSession).thenReturn(engineSession)

        val webchannelFeature =
            spy(FxaWebChannelFeature(testContext, null, mock(), sessionManager, accountManager, setOf(FxaCapability.CHOOSE_WHAT_TO_SYNC)))
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
        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())
        assertTrue(responseToTheWebChannel.value.getCWTSSupport()!!)
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
        val expectedEngines = setOf(SyncEngine.History)

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

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

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(capabilitiesFromWebChannel.size == 1)
        assertNull(responseToTheWebChannel.value.getCWTSSupport())

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
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords)

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

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

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    // Receiving and responding a fxa-status message if account manager is logged in
    @Test
    fun `COMMAND_STATUS with account manager is logged in with profile`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords)
        val logoutDeferred = CompletableDeferred<Unit>()

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val account = mock<OAuthAccount>()
        val profile = Profile(uid = "testUID", email = "test@example.com", avatar = null, displayName = null)
        whenever(account.getSessionToken()).thenReturn("testToken")
        whenever(accountManager.accountProfile()).thenReturn(profile)
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
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

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertNull(responseToTheWebChannel.value.getCWTSSupport())

        val signedInUser = responseToTheWebChannel.value.signedInUser()
        assertEquals("test@example.com", signedInUser.email)
        assertEquals("testUID", signedInUser.uid)
        assertTrue(signedInUser.verified)
        assertEquals("testToken", signedInUser.sessionToken)
    }

    @Test
    fun `COMMAND_STATUS with account manager is logged in without profile`() {
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val session = mock<Session>()
        val engineSession = mock<EngineSession>()
        val ext = mock<WebExtension>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val port = mock<Port>()
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords)
        val logoutDeferred = CompletableDeferred<Unit>()

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val account = mock<OAuthAccount>()
        whenever(account.getSessionToken()).thenReturn("testToken")
        whenever(accountManager.accountProfile()).thenReturn(null)
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
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

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertNull(responseToTheWebChannel.value.getCWTSSupport())

        val signedInUser = responseToTheWebChannel.value.signedInUser()
        assertNull(signedInUser.email)
        assertNull(signedInUser.uid)
        assertTrue(signedInUser.verified)
        assertEquals("testToken", signedInUser.sessionToken)
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
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords)

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

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

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
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
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords)

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

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

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(expectedEngines.all {
            capabilitiesFromWebChannel.contains(it.nativeName)
        })

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
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

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

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

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
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

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext
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
        verifyOauthLogin("signin", AuthType.Signin, "fffs", "fsdf32", null, messageHandler.value, accountManager)
        // Signup.
        verifyOauthLogin("signup", AuthType.Signup, "anotherCode1", "anotherState2", setOf(SyncEngine.Passwords), messageHandler.value, accountManager)
        // Pairing.
        verifyOauthLogin("pairing", AuthType.Pairing, "anotherCode2", "anotherState3", null, messageHandler.value, accountManager)
        // Some other action.
        verifyOauthLogin("newAction", AuthType.OtherExternal("newAction"), "anotherCode3", "anotherState4", null, messageHandler.value, accountManager)
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

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext
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
        verifyOauthLogin("signin", AuthType.Signin, "fffs", "fsdf32", setOf(SyncEngine.Passwords, SyncEngine.Bookmarks), messageHandler.value, accountManager)
        // Signup.
        verifyOauthLogin("signup", AuthType.Signup, "anotherCode1", "anotherState2", null, messageHandler.value, accountManager)
        // Pairing.
        verifyOauthLogin("pairing", AuthType.Pairing, "anotherCode2", "anotherState3", null, messageHandler.value, accountManager)
        // Some other action.
        verifyOauthLogin("newAction", AuthType.OtherExternal("newAction"), "anotherCode3", "anotherState4", null, messageHandler.value, accountManager)
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
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks)

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

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

        messageHandler.value.onPortMessage(jsonToWebChannel, port)
        verify(port).postMessage(jsonFromWebChannel.capture())

        assertTrue(jsonFromWebChannel.value.getOk())
    }

    private fun prepareFeatureForTest(
        port: Port,
        session: Session = mock(),
        engineSession: EngineSession = mock(),
        controller: WebExtensionController? = null
    ): FxaWebChannelFeature {
        val engine = mock<Engine>()
        val sessionManager = mock<SessionManager>()
        val accountManager = mock<FxaAccountManager>()
        val ext = mock<WebExtension>()

        whenever(ext.getConnectedPort(eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID), any())).thenReturn(port)
        whenever(sessionManager.selectedSession).thenReturn(session)
        whenever(sessionManager.getEngineSession(session)).thenReturn(engineSession)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val feature = FxaWebChannelFeature(testContext, null, engine, sessionManager, accountManager)
        if (controller != null) {
            feature.extensionController = controller
        }
        return feature
    }

    private fun JSONObject.getSupportedEngines(): List<String> {
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

    private fun JSONObject.getCWTSSupport(): Boolean? {
        return try {
            this.getJSONObject("message")
                .getJSONObject("data")
                .getJSONObject("capabilities")
                .getBoolean("choose_what_to_sync")
        } catch (e: JSONException) {
            null
        }
    }

    data class SignedInUser(val email: String?, val uid: String?, val sessionToken: String, val verified: Boolean)

    private fun JSONObject.signedInUser(): SignedInUser {
        val obj = this.getJSONObject("message")
            .getJSONObject("data")
            .getJSONObject("signedInUser")

        val email = if (obj.getString("email") == "null") {
            null
        } else {
            obj.getString("email")
        }
        val uid = if (obj.getString("uid") == "null") {
            null
        } else {
            obj.getString("uid")
        }
        return SignedInUser(
            email = email,
            uid = uid,
            sessionToken = obj.getString("sessionToken"),
            verified = obj.getBoolean("verified")
        )
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

    private fun verifyOauthLogin(action: String, expectedAuthType: AuthType, code: String, state: String, declined: Set<SyncEngine>?, messageHandler: MessageHandler, accountManager: FxaAccountManager) {
        val jsonToWebChannel = jsonOauthLogin(action, code, state, declined ?: emptySet())
        messageHandler.onPortMessage(jsonToWebChannel, mock())

        val expectedAuthData = FxaAuthData(
            authType = expectedAuthType,
            code = code,
            state = state,
            declinedEngines = declined ?: emptySet()
        )
        verify(accountManager).finishAuthenticationAsync(expectedAuthData)
    }

    private fun jsonOauthLogin(action: String, code: String, state: String, declined: Set<SyncEngine>): JSONObject {
        return JSONObject(
            """{
             "message":{
                "command": "fxaccounts:oauth_login",
                "messageId":123,
                "data":{
                    "action":"$action",
                    "redirect":"urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel",
                    "code":"$code",
                    "state":"$state",
                    "declinedSyncEngines":${declined.map { "${it.nativeName}," }.filterNotNull()}
                }
             }
            }""".trimIndent()
        )
    }
}
