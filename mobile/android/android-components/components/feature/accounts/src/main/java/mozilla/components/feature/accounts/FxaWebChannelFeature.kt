/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.concept.sync.AuthType
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.Server
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.toSyncEngines
import mozilla.components.service.fxa.toAuthType
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.isSameOriginAs
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.support.webextensions.WebExtensionController
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.lang.ClassCastException
import java.net.URL

/**
 * Configurable FxA capabilities.
 */
enum class FxaCapability {
    // Enables "choose what to sync" selection during support auth flows (currently, sign-up).
    CHOOSE_WHAT_TO_SYNC,
}

/**
 * Feature implementation that provides Firefox Accounts WebChannel support.
 * For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
 * This feature uses a web extension to communicate with FxA Web Content.
 *
 * @property customTabSessionId optional custom tab session ID, if feature is being used with a custom tab.
 * @property runtime the [WebExtensionRuntime] (e.g the browser engine) to use.
 * @property store a reference to the application's [BrowserStore].
 * @property accountManager a reference to application's [FxaAccountManager].
 * @property fxaCapabilities a set of [FxaCapability] that client supports.
 */
class FxaWebChannelFeature(
    private val customTabSessionId: String?,
    private val runtime: WebExtensionRuntime,
    private val store: BrowserStore,
    private val accountManager: FxaAccountManager,
    private val serverConfig: ServerConfig,
    private val fxaCapabilities: Set<FxaCapability> = emptySet(),
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    @VisibleForTesting
    // This is an internal var to make it mutable for unit testing purposes only
    internal var extensionController = WebExtensionController(
        WEB_CHANNEL_EXTENSION_ID,
        WEB_CHANNEL_EXTENSION_URL,
        WEB_CHANNEL_MESSAGING_ID,
    )

    override fun start() {
        val messageHandler = WebChannelViewBackgroundMessageHandler(serverConfig)
        extensionController.registerBackgroundMessageHandler(messageHandler, WEB_CHANNEL_BACKGROUND_MESSAGING_ID)

        extensionController.install(runtime)

        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findCustomTabOrSelectedTab(customTabSessionId) }
                .ifChanged { it.engineState.engineSession }
                .collect {
                    it.engineState.engineSession?.let { engineSession ->
                        registerFxaContentMessageHandler(engineSession)
                    }
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    @Suppress("MaxLineLength", "")
    /* ktlint-disable no-multi-spaces */
    /**
     * Communication channel is established from fxa-web-content to this class via webextension, as follows:
     * [fxa-web-content] <--js events--> [fxawebchannel.js webextension] <--port messages--> [FxaWebChannelFeature]
     *
     * Overall message flow, as implemented by this class, is documented below. For detailed message descriptions, see:
     * https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
     *
     * [fxa-web-channel]            [FxaWebChannelFeature]         Notes:
     *     loaded           ------>          |                  fxa web content loaded
     *     fxa-status       ------>          |                  web content requests account status & device capabilities
     *        |             <------ fxa-status-response         this class responds, based on state of [accountManager]
     *     can-link-account ------>          |                  user submitted credentials, web content verifying if account linking is allowed
     *        |             <------ can-link-account-response   this class responds, based on state of [accountManager]
     *     oauth-login      ------>                             authentication completed within fxa web content, this class receives OAuth code & state
     */
    private class WebChannelViewContentMessageHandler(
        private val accountManager: FxaAccountManager,
        private val serverConfig: ServerConfig,
        private val fxaCapabilities: Set<FxaCapability>,
    ) : MessageHandler {
        @SuppressWarnings("ComplexMethod")
        override fun onPortMessage(message: Any, port: Port) {
            if (!isCommunicationAllowed(serverConfig, port)) {
                logger.error("Communication disallowed, ignoring WebChannel message.")
                return
            }

            val json = try {
                message as JSONObject
            } catch (e: ClassCastException) {
                logger.error("Received an invalid WebChannel message of type: ${message.javaClass}")
                // TODO ideally, this should log to Sentry
                return
            }

            val payload: JSONObject
            val command: WebChannelCommand
            val messageId: String

            try {
                payload = json.getJSONObject("message")
                command = payload.getString("command").toWebChannelCommand() ?: throw
                JSONException("Couldn't get WebChannel command")
                messageId = payload.optString("messageId", "")
            } catch (e: JSONException) {
                // We don't have control over what messages we will get from the webchannel.
                // If somehow we're receiving mis-constructed messages, it's probably best to not
                // blow up the host application. This comes at a cost: we might not catch problems
                // as quickly if we're not crashing (and thus receiving crash logs).
                // TODO ideally, this should log to Sentry.
                logger.error("Error while processing WebChannel command", e)
                return
            }

            logger.debug("Processing WebChannel command: $command")

            val response = when (command) {
                WebChannelCommand.CAN_LINK_ACCOUNT -> processCanLinkAccountCommand(messageId)
                WebChannelCommand.FXA_STATUS -> processFxaStatusCommand(accountManager, messageId, fxaCapabilities)
                WebChannelCommand.OAUTH_LOGIN -> processOauthLoginCommand(accountManager, payload)
            }
            response?.let { port.postMessage(it) }
        }
    }

    private fun registerFxaContentMessageHandler(engineSession: EngineSession) {
        val messageHandler = WebChannelViewContentMessageHandler(accountManager, serverConfig, fxaCapabilities)
        extensionController.registerContentMessageHandler(engineSession, messageHandler)
    }

    private class WebChannelViewBackgroundMessageHandler(
        private val serverConfig: ServerConfig,
    ) : MessageHandler {
        override fun onPortConnected(port: Port) {
            if (Server.values().none { it.contentUrl == serverConfig.contentUrl }) {
                port.postMessage(JSONObject().put("type", "overrideFxAServer").put("url", serverConfig.contentUrl))
            }
        }
    }

    @VisibleForTesting
    companion object {
        private val logger = Logger("mozac-fxawebchannel")

        internal const val WEB_CHANNEL_EXTENSION_ID = "fxa@mozac.org"
        internal const val WEB_CHANNEL_MESSAGING_ID = "mozacWebchannel"
        internal const val WEB_CHANNEL_BACKGROUND_MESSAGING_ID = "mozacWebchannelBackground"
        internal const val WEB_CHANNEL_EXTENSION_URL = "resource://android/assets/extensions/fxawebchannel/"

        // Constants for incoming messages from the WebExtension.
        private const val CHANNEL_ID = "account_updates"

        enum class WebChannelCommand {
            CAN_LINK_ACCOUNT,
            OAUTH_LOGIN,
            FXA_STATUS,
        }

        // For all possible messages and their meaning/payloads, see:
        // https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md

        /**
         * Gets triggered when user initiates a login within FxA web content.
         * Expects a response.
         * On Fx Desktop, this event triggers "a different user was previously signed in on this machine" warning.
         */
        private const val COMMAND_CAN_LINK_ACCOUNT = "fxaccounts:can_link_account"

        /**
         * Gets triggered when a user successfully authenticates via OAuth.
         */
        private const val COMMAND_OAUTH_LOGIN = "fxaccounts:oauth_login"

        /**
         * Gets triggered on startup to fetch the FxA state from the host application.
         * Expects a response, which includes application's capabilities and a description of the
         * current Firefox Account (if present).
         */
        private const val COMMAND_STATUS = "fxaccounts:fxa_status"

        /**
         * Handles the [COMMAND_CAN_LINK_ACCOUNT] event from the web-channel.
         * Currently this always response with 'ok=true'.
         * On Fx Desktop, this event prompts a possible "another user was previously logged in on
         * this device" warning. Currently we don't support propagating this warning to a consuming application.
         */
        private fun processCanLinkAccountCommand(messageId: String): JSONObject {
            // TODO don't allow linking if we're logged in already? This is requested after user
            // entered their credentials.
            return JSONObject().also { status ->
                status.put("id", CHANNEL_ID)
                status.put(
                    "message",
                    JSONObject().also { message ->
                        message.put("messageId", messageId)
                        message.put("command", COMMAND_CAN_LINK_ACCOUNT)
                        message.put(
                            "data",
                            JSONObject().also { data ->
                                data.put("ok", true)
                            },
                        )
                    },
                )
            }
        }

        /**
         * Handles the [COMMAND_STATUS] event from the web-channel.
         * Responds with supported application capabilities and information about currently signed-in Firefox Account.
         */
        @Suppress("ComplexMethod")
        private fun processFxaStatusCommand(
            accountManager: FxaAccountManager,
            messageId: String,
            fxaCapabilities: Set<FxaCapability>,
        ): JSONObject {
            val status =  JSONObject()
            status.put("id", CHANNEL_ID)
            status.put(
                "message",
                JSONObject().also { message ->
                    message.put("messageId", messageId)
                    message.put("command", COMMAND_STATUS)
                    message.put(
                        "data",
                        JSONObject().also { data ->
                            data.put(
                                "capabilities",
                                JSONObject().also { capabilities ->
                                    capabilities.put(
                                        "engines",
                                        JSONArray().also { engines ->
                                            accountManager.supportedSyncEngines()?.forEach { engine ->
                                                engines.put(engine.nativeName)
                                            } ?: emptyArray<SyncEngine>()
                                        },
                                    )

                                    if (fxaCapabilities.contains(FxaCapability.CHOOSE_WHAT_TO_SYNC)) {
                                        capabilities.put("choose_what_to_sync", true)
                                    }
                                },
                            )
                            val account = accountManager.authenticatedAccount()
                            if (account == null) {
                                data.put("signedInUser", JSONObject.NULL)
                            } else {
                                data.put(
                                    "signedInUser",
                                    JSONObject().also { signedInUser ->
                                        signedInUser.put(
                                            "email",
                                            accountManager.accountProfile()?.email ?: JSONObject.NULL,
                                        )
                                        signedInUser.put(
                                            "uid",
                                            accountManager.accountProfile()?.uid ?: JSONObject.NULL,
                                        )
                                        signedInUser.put(
                                            "sessionToken",
                                            account.getSessionToken() ?: JSONObject.NULL,
                                        )
                                        // Our account state machine only ever completes authentication for
                                        // "verified" accounts, so this is always 'true'.
                                        signedInUser.put(
                                            "verified",
                                            true,
                                        )
                                    },
                                )
                            }
                        },
                    )
                },
            )
            return status
        }

        private fun JSONArray.toStringList(): List<String> {
            val result = mutableListOf<String>()
            for (i in 0 until this.length()) {
                this.optString(i, null)?.let { result.add(it) }
            }
            return result
        }

        /**
         * Handles the [COMMAND_OAUTH_LOGIN] event from the web-channel.
         */
        private fun processOauthLoginCommand(accountManager: FxaAccountManager, payload: JSONObject): JSONObject? {
            val authType: AuthType
            val code: String
            val state: String
            val declinedEngines: List<String>?

            try {
                val data = payload.getJSONObject("data")
                authType = data.getString("action").toAuthType()
                code = data.getString("code")
                state = data.getString("state")
                declinedEngines = data.optJSONArray("declinedSyncEngines")?.toStringList()
            } catch (e: JSONException) {
                // TODO ideally, this should log to Sentry.
                logger.error("Error while processing WebChannel oauth-login command", e)
                return null
            }

            CoroutineScope(Dispatchers.Main).launch {
                accountManager.finishAuthentication(
                    FxaAuthData(
                        authType = authType,
                        code = code,
                        state = state,
                        declinedEngines = declinedEngines?.toSyncEngines(),
                    ),
                )
            }

            return null
        }

        private fun String.toWebChannelCommand(): WebChannelCommand? {
            return when (this) {
                COMMAND_CAN_LINK_ACCOUNT -> WebChannelCommand.CAN_LINK_ACCOUNT
                COMMAND_OAUTH_LOGIN -> WebChannelCommand.OAUTH_LOGIN
                COMMAND_STATUS -> WebChannelCommand.FXA_STATUS
                else -> {
                    logger.warn("Unrecognized WebChannel command: $this")
                    null
                }
            }
        }

        private fun isCommunicationAllowed(serverConfig: ServerConfig, port: Port): Boolean {
            val senderOrigin = port.senderUrl()
            val expectedOrigin = serverConfig.contentUrl
            return isCommunicationAllowed(senderOrigin, expectedOrigin)
        }

        @VisibleForTesting
        internal fun isCommunicationAllowed(senderOrigin: String, expectedOrigin: String): Boolean {
            if (!isSafeUrl(senderOrigin)) {
                logger.error("$senderOrigin looks unsafe, aborting.")
                return false
            }

            if (!senderOrigin.isSameOriginAs(expectedOrigin)) {
                logger.error("Host mismatch for WebChannel message. Expected: $expectedOrigin, got: $senderOrigin.")
                return false
            }
            return true
        }

        /**
         * Rejects URLs that are deemed "unsafe" (not expected).
         */
        private fun isSafeUrl(urlStr: String): Boolean {
            val url = URL(urlStr)
            return url.userInfo.isNullOrEmpty() &&
                (url.protocol == "https" || url.host == "localhost" || url.host == "127.0.0.1")
        }
    }
}
