/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.os.Looper.getMainLooper
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.Server
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.support.webextensions.WebExtensionController
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class FxaWebChannelFeatureTest {

    @Before
    fun setup() {
        WebExtensionController.installedExtensions.clear()
    }

    @Test
    fun `start installs webextension`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val accountManager: FxaAccountManager = mock()
        val serverConfig: ServerConfig = mock()
        val webchannelFeature = FxaWebChannelFeature(null, engine, store, accountManager, serverConfig)
        webchannelFeature.start()

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_URL),
            onSuccess.capture(),
            onError.capture(),
        )

        onSuccess.value.invoke(mock())

        // Already installed, should not try to install again.
        webchannelFeature.start()
        verify(engine, times(1)).installWebExtension(
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID),
            eq(FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_URL),
            any(),
            any(),
        )
    }

    @Test
    fun `start registers the background message handler`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val accountManager: FxaAccountManager = mock()
        val serverConfig: ServerConfig = mock()
        val controller: WebExtensionController = mock()
        val webchannelFeature = FxaWebChannelFeature(null, engine, store, accountManager, serverConfig)

        webchannelFeature.extensionController = controller

        webchannelFeature.start()

        verify(controller).registerBackgroundMessageHandler(any(), any())
    }

    @Test
    fun `backgroundMessageHandler sends overrideFxAServer`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val accountManager: FxaAccountManager = mock()
        val serverConfig: ServerConfig = mock()
        val controller: WebExtensionController = mock()
        val webchannelFeature = FxaWebChannelFeature(null, engine, store, accountManager, serverConfig)

        whenever(serverConfig.contentUrl).thenReturn("https://foo.bar")
        webchannelFeature.extensionController = controller

        webchannelFeature.start()

        val messageHandler = argumentCaptor<MessageHandler>()
        verify(controller).registerBackgroundMessageHandler(messageHandler.capture(), any())

        val port: Port = mock()
        val message = argumentCaptor<JSONObject>()
        messageHandler.value.onPortConnected(port)
        verify(port).postMessage(message.capture())

        val overrideUrlMessage = JSONObject().put("type", "overrideFxAServer").put("url", "https://foo.bar")
        verify(port, times(1)).postMessage(message.capture())

        assertEquals(overrideUrlMessage.toString(), message.value.toString())
    }

    @Test
    fun `backgroundMessageHandler should not send overrideFxAServer for predefined Config`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val accountManager: FxaAccountManager = mock()
        val serverConfig: ServerConfig = mock()
        val controller: WebExtensionController = mock()
        val webchannelFeature = FxaWebChannelFeature(null, engine, store, accountManager, serverConfig)

        whenever(serverConfig.contentUrl).thenReturn(Server.RELEASE.contentUrl)
        webchannelFeature.extensionController = controller

        webchannelFeature.start()

        val messageHandler = argumentCaptor<MessageHandler>()
        verify(controller).registerBackgroundMessageHandler(messageHandler.capture(), any())

        val port: Port = mock()
        messageHandler.value.onPortConnected(port)

        verify(port, never()).postMessage(any())
    }

    @Test
    fun `start registers content message handler for selected session`() {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        val accountManager: FxaAccountManager = mock()
        val serverConfig: ServerConfig = mock()
        val controller: WebExtensionController = mock()

        val tab = createTab("https://www.mozilla.org", id = "test-tab", engineSession = engineSession)
        val store = spy(
            BrowserStore(initialState = BrowserState(tabs = listOf(tab), selectedTabId = tab.id)),
        )

        val webchannelFeature = FxaWebChannelFeature(null, engine, store, accountManager, serverConfig)
        webchannelFeature.extensionController = controller

        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(controller).registerContentMessageHandler(eq(engineSession), any(), any())
    }

    @Test
    fun `Ignores messages coming from a different FxA host than configured`() {
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines: Set<SyncEngine> = setOf(SyncEngine.History)
        val messageHandler = argumentCaptor<MessageHandler>()
        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines)
        whenever(port.senderUrl()).thenReturn("https://bar.foo/email")
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port, never()).postMessage(any())
    }

    @Test
    fun `COMMAND_STATUS configured with CWTS must provide a boolean=true flag to the web-channel`() {
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines: Set<SyncEngine> = setOf(SyncEngine.History)
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val webchannelFeature = prepareFeatureForTest(
            ext,
            port,
            engineSession,
            expectedEngines,
            setOf(FxaCapability.CHOOSE_WHAT_TO_SYNC),
        )
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )
        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())
        assertTrue(responseToTheWebChannel.value.getCWTSSupport()!!)
    }

    // Receiving and responding a fxa-status message if sync is configured with one engine
    @Test
    fun `COMMAND_STATUS configured with one engine must be provided to the web-channel`() {
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines: Set<SyncEngine> = setOf(SyncEngine.History)
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
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
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines: Set<SyncEngine> = setOf(SyncEngine.History)
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()
        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(
            expectedEngines.all {
                capabilitiesFromWebChannel.contains(it.nativeName)
            },
        )

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    // Receiving and responding a fxa-status message if account manager is logged in
    @Test
    fun `COMMAND_STATUS with account manager is logged in with profile`() {
        val accountManager: FxaAccountManager = mock()
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines: Set<SyncEngine> = setOf(SyncEngine.History)
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()

        val account: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", email = "test@example.com", avatar = null, displayName = null)
        whenever(account.getSessionToken()).thenReturn("testToken")
        whenever(accountManager.accountProfile()).thenReturn(profile)
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(
            expectedEngines.all {
                capabilitiesFromWebChannel.contains(it.nativeName)
            },
        )

        assertNull(responseToTheWebChannel.value.getCWTSSupport())

        val signedInUser = responseToTheWebChannel.value.signedInUser()
        assertEquals("test@example.com", signedInUser.email)
        assertEquals("testUID", signedInUser.uid)
        assertTrue(signedInUser.verified)
        assertEquals("testToken", signedInUser.sessionToken)
    }

    @Test
    fun `COMMAND_STATUS with account manager is logged in without profile`() {
        val accountManager: FxaAccountManager = mock()
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines: Set<SyncEngine> = setOf(SyncEngine.History)
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()

        val account: OAuthAccount = mock()
        whenever(account.getSessionToken()).thenReturn("testToken")
        whenever(accountManager.accountProfile()).thenReturn(null)
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(
            expectedEngines.all {
                capabilitiesFromWebChannel.contains(it.nativeName)
            },
        )

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
        val accountManager: FxaAccountManager = mock()
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords)
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()

        whenever(accountManager.accountProfile()).thenReturn(null)
        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(
            expectedEngines.all {
                capabilitiesFromWebChannel.contains(it.nativeName)
            },
        )

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    // Receiving and responding a fxa-status message if account manager when sync is not configured
    @Test
    fun `COMMAND_STATUS with account manager when sync is not configured`() {
        val accountManager: FxaAccountManager = mock() // syncConfig is null by default (is not configured)
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords)
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()

        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(
            expectedEngines.all {
                capabilitiesFromWebChannel.contains(it.nativeName)
            },
        )

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
        assertTrue(responseToTheWebChannel.value.isSignedInUserNull())
    }

    @Test
    fun `COMMAND_STATUS with no capabilities configured must provide an empty list of engines to the web-channel`() {
        val accountManager: FxaAccountManager = mock() // syncConfig is null by default (is not configured)
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val messageHandler = argumentCaptor<MessageHandler>()
        val responseToTheWebChannel = argumentCaptor<JSONObject>()

        whenever(accountManager.supportedSyncEngines()).thenReturn(null)
        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, null, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val requestFromTheWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:fxa_status",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(requestFromTheWebChannel, port)
        verify(port).postMessage(responseToTheWebChannel.capture())

        assertNull(responseToTheWebChannel.value.getCWTSSupport())
        val capabilitiesFromWebChannel = responseToTheWebChannel.value.getSupportedEngines()
        assertTrue(capabilitiesFromWebChannel.isEmpty())
    }

    // Receiving an oauth-login message account manager accepts the request
    @Test
    fun `COMMAND_OAUTH_LOGIN web-channel must be processed through when the accountManager accepts the request`() = runTest {
        val accountManager: FxaAccountManager = mock() // syncConfig is null by default (is not configured)
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val messageHandler = argumentCaptor<MessageHandler>()

        whenever(accountManager.finishAuthentication(any())).thenReturn(false)
        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, null, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
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
    fun `COMMAND_OAUTH_LOGIN web-channel must be processed when the accountManager refuses the request`() = runTest {
        val accountManager: FxaAccountManager = mock() // syncConfig is null by default (is not configured)
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val messageHandler = argumentCaptor<MessageHandler>()

        whenever(accountManager.finishAuthentication(any())).thenReturn(false)
        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, null, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
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
        val accountManager: FxaAccountManager = mock() // syncConfig is null by default (is not configured)
        val engineSession: EngineSession = mock()
        val ext: WebExtension = mock()
        val port: Port = mock()
        val jsonFromWebChannel = argumentCaptor<JSONObject>()
        val messageHandler = argumentCaptor<MessageHandler>()
        val expectedEngines = setOf(SyncEngine.History, SyncEngine.Bookmarks)

        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val webchannelFeature = prepareFeatureForTest(ext, port, engineSession, expectedEngines, emptySet(), accountManager)
        webchannelFeature.start()
        shadowOf(getMainLooper()).idle()

        verify(ext).registerContentMessageHandler(
            eq(engineSession),
            eq(FxaWebChannelFeature.WEB_CHANNEL_MESSAGING_ID),
            messageHandler.capture(),
        )
        messageHandler.value.onPortConnected(port)

        val jsonToWebChannel = JSONObject(
            """{
             "message":{
                "command": "fxaccounts:can_link_account",
                "messageId":123
             }
            }
            """.trimIndent(),
        )

        messageHandler.value.onPortMessage(jsonToWebChannel, port)
        verify(port).postMessage(jsonFromWebChannel.capture())

        assertTrue(jsonFromWebChannel.value.getOk())
    }

    @Test
    fun `isCommunicationAllowed extensive testing`() {
        // Unsafe URL: not https.
        assertFalse(FxaWebChannelFeature.isCommunicationAllowed("http://foo.bar", "http://foo.bar"))
        // Unsafe URL: login in url.
        assertFalse(FxaWebChannelFeature.isCommunicationAllowed("http://bobo:bobo@foo.bar", "http://foo.bar"))
        // Origin mismatch.
        assertFalse(FxaWebChannelFeature.isCommunicationAllowed("https://foo.bar", "https://foo.baz"))

        // Happy cases
        assertTrue(FxaWebChannelFeature.isCommunicationAllowed("https://foo.bar", "https://foo.bar"))
        // HTTP is allowed for localhost.
        assertTrue(FxaWebChannelFeature.isCommunicationAllowed("http://127.0.0.1", "http://127.0.0.1"))
        assertTrue(FxaWebChannelFeature.isCommunicationAllowed("http://localhost", "http://localhost"))
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
            verified = obj.getBoolean("verified"),
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

    private suspend fun verifyOauthLogin(action: String, expectedAuthType: AuthType, code: String, state: String, declined: Set<SyncEngine>?, messageHandler: MessageHandler, accountManager: FxaAccountManager) {
        val jsonToWebChannel = jsonOauthLogin(action, code, state, declined ?: emptySet())
        val port = mock<Port>()
        whenever(port.senderUrl()).thenReturn("https://foo.bar/email")
        messageHandler.onPortMessage(jsonToWebChannel, port)

        val expectedAuthData = FxaAuthData(
            authType = expectedAuthType,
            code = code,
            state = state,
            declinedEngines = declined ?: emptySet(),
        )
        shadowOf(getMainLooper()).idle()

        verify(accountManager).finishAuthentication(expectedAuthData)
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
            }
            """.trimIndent(),
        )
    }

    private fun prepareFeatureForTest(
        ext: WebExtension = mock(),
        port: Port = mock(),
        engineSession: EngineSession = mock(),
        expectedEngines: Set<SyncEngine>? = setOf(SyncEngine.History),
        fxaCapabilities: Set<FxaCapability> = emptySet(),
        accountManager: FxaAccountManager = mock(),
    ): FxaWebChannelFeature {
        val serverConfig: ServerConfig = mock()
        WebExtensionController.installedExtensions[FxaWebChannelFeature.WEB_CHANNEL_EXTENSION_ID] = ext

        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "test-tab",
            engineSession = engineSession,
        )
        val store = spy(
            BrowserStore(
                initialState = BrowserState(tabs = listOf(tab), selectedTabId = tab.id),
            ),
        )

        whenever(accountManager.supportedSyncEngines()).thenReturn(expectedEngines)
        whenever(port.engineSession).thenReturn(engineSession)
        whenever(port.senderUrl()).thenReturn("https://foo.bar/email")
        whenever(serverConfig.contentUrl).thenReturn("https://foo.bar")

        return spy(FxaWebChannelFeature(null, mock(), store, accountManager, serverConfig, fxaCapabilities))
    }
}
