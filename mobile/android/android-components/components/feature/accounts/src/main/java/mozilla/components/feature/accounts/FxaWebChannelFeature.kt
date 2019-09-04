/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSessionIdOrSelected
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.sync.AuthType
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.toAuthType
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.lang.ClassCastException
import java.util.WeakHashMap

/**
 * Feature implementation that provides Firefox Accounts WebChannel support.
 * For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
 * This feature uses a web extension to communicate with FxA Web Content.
 *
 * Boilerplate around installing and communicating with the extension will be cleaned up as part https://github.com/mozilla-mobile/android-components/issues/4297.
 *
 * @property context a reference to the context.
 * @property customTabSessionId optional custom tab session ID, if feature is being used with a custom tab.
 * @property engine a reference to application's browser engine.
 * @property sessionManager a reference to application's [SessionManager].
 * @property accountManager a reference to application's [FxaAccountManager].
 */
@Suppress("TooManyFunctions")
class FxaWebChannelFeature(
    private val context: Context,
    private val customTabSessionId: String?,
    private val engine: Engine,
    private val sessionManager: SessionManager,
    private val accountManager: FxaAccountManager
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature {

    override fun start() {
        // Runs observeSelected (if we're not in a custom tab) or observeFixed (if we are).
        observeIdOrSelected(customTabSessionId)

        sessionManager.runWithSessionIdOrSelected(customTabSessionId) { session ->
            registerContentMessageHandler(session)
        }

        if (installedWebExt == null) {
            install(engine)
        }
    }

    override fun onSessionAdded(session: Session) {
        registerContentMessageHandler(session)
    }

    override fun onSessionRemoved(session: Session) {
        ports.remove(sessionManager.getEngineSession(session))
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
        private val engineSession: EngineSession,
        private val accountManager: FxaAccountManager
    ) : MessageHandler {
        override fun onPortConnected(port: Port) {
            ports[port.engineSession] = port
        }

        override fun onPortDisconnected(port: Port) {
            ports.remove(port.engineSession)
        }

        override fun onPortMessage(message: Any, port: Port) {
            val json = try {
                message as JSONObject
            } catch (e: ClassCastException) {
                logger.error("Received an invalid WebChannel message of type: ${message.javaClass}")
                // TODO ideally, this should log to Sentry
                return
            }

            val payload: JSONObject
            val command: WebChannelCommand?
            val messageId: String

            try {
                payload = json.getJSONObject("message")
                command = payload.getString("command").toWebChannelCommand()
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

            if (command == null) {
                // TODO ideally, this should log to Sentry.
                logger.error("Couldn't get WebChannel command")
                return
            }

            logger.debug("Processing WebChannel command: $command")
            when (command) {
                WebChannelCommand.CAN_LINK_ACCOUNT -> processCanLinkAccountCommand(
                    messageId, engineSession
                )
                WebChannelCommand.FXA_STATUS -> processFxaStatusCommand(
                    accountManager, messageId, engineSession
                )
                WebChannelCommand.OAUTH_LOGIN -> processOauthLoginCommand(
                    accountManager, payload
                )
            }
        }
    }

    private fun registerContentMessageHandler(session: Session) {
        val engineSession = sessionManager.getOrCreateEngineSession(session)
        val messageHandler = WebChannelViewContentMessageHandler(engineSession, accountManager)
        registerMessageHandler(engineSession, messageHandler)
    }

    @VisibleForTesting
    companion object {
        private val logger = Logger("mozac-fxawebchannel")

        internal const val WEB_CHANNEL_EXTENSION_ID = "mozacWebchannel"
        internal const val WEB_CHANNEL_EXTENSION_URL =
            "resource://android/assets/extensions/fxawebchannel/"

        // Constants for incoming messages from the WebExtension.
        private const val CHANNEL_ID = "account_updates"

        enum class WebChannelCommand {
            CAN_LINK_ACCOUNT,
            OAUTH_LOGIN,
            FXA_STATUS
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

        @Volatile
        internal var installedWebExt: WebExtension? = null

        @Volatile
        private var registerContentMessageHandler: (WebExtension) -> Unit? = { }

        internal var ports = WeakHashMap<EngineSession, Port>()

        /**
         * Installs the WebChannel web extension in the provided engine.
         *
         * @param engine a reference to the application's browser engine.
         */
        fun install(engine: Engine) {
            engine.installWebExtension(WEB_CHANNEL_EXTENSION_ID, WEB_CHANNEL_EXTENSION_URL,
                    onSuccess = {
                        logger.debug("Installed extension: ${it.id}")
                        registerContentMessageHandler(it)
                        installedWebExt = it
                    },
                    onError = { ext, throwable ->
                        logger.error("Failed to install extension: $ext", throwable)
                    }
            )
        }

        fun registerMessageHandler(session: EngineSession, messageHandler: MessageHandler) {
            registerContentMessageHandler = {
                if (!it.hasContentMessageHandler(session, WEB_CHANNEL_EXTENSION_ID)) {
                    it.registerContentMessageHandler(session, WEB_CHANNEL_EXTENSION_ID, messageHandler)
                }
            }

            installedWebExt?.let { registerContentMessageHandler(it) }
        }

        /**
         * Handles the [COMMAND_CAN_LINK_ACCOUNT] event from the web-channel.
         * Currently this always response with 'ok=true'.
         * On Fx Desktop, this event prompts a possible "another user was previously logged in on
         * this device" warning. Currently we don't support propagating this warning to a consuming application.
         */
        private fun processCanLinkAccountCommand(messageId: String, engineSession: EngineSession) {
            // TODO don't allow linking if we're logged in already? This is requested after user
            // entered their credentials.
            val status = JSONObject().also { status ->
                status.put("id", CHANNEL_ID)
                status.put("message", JSONObject().also { message ->
                    message.put("messageId", messageId)
                    message.put("command", COMMAND_CAN_LINK_ACCOUNT)
                    message.put("data", JSONObject().also { data ->
                        data.put("ok", true)
                    })
                })
            }

            sendContentMessage(status, engineSession)
        }

        /**
         * Handles the [COMMAND_STATUS] event from the web-channel.
         * Responds with supported application capabilities and information about currently signed-in Firefox Account.
         */
        private fun processFxaStatusCommand(
            accountManager: FxaAccountManager,
            messageId: String,
            engineSession: EngineSession
        ) {
            val status = JSONObject().also { status ->
                status.put("id", CHANNEL_ID)
                status.put("message", JSONObject().also { message ->
                    message.put("messageId", messageId)
                    message.put("command", COMMAND_STATUS)
                    message.put("data", JSONObject().also { data ->
                        data.put("capabilities", JSONObject().also { capabilities ->
                            capabilities.put("engines", JSONArray().also { engines ->
                                accountManager.supportedSyncEngines()?.forEach { engine ->
                                    engines.put(engine.nativeName)
                                } ?: emptyArray<SyncEngine>()
                            })
                        })
                        // Since accountManager currently can't provide us with a sessionToken, this is
                        // hard-coded to null.
                        // Fix this once https://github.com/mozilla/application-services/issues/1669 is resolved.
                        data.put("signedInUser", JSONObject.NULL)
                    })
                })
            }

            sendContentMessage(status, engineSession)
        }

        /**
         * Handles the [COMMAND_OAUTH_LOGIN] event from the web-channel.
         */
        private fun processOauthLoginCommand(accountManager: FxaAccountManager, payload: JSONObject) {
            val authType: AuthType
            val code: String
            val state: String

            try {
                val data = payload.getJSONObject("data")
                authType = data.getString("action").toAuthType()
                code = data.getString("code")
                state = data.getString("state")
            } catch (e: JSONException) {
                // TODO ideally, this should log to Sentry.
                logger.error("Error while processing WebChannel oauth-login command", e)
                return
            }

            accountManager.finishAuthenticationAsync(FxaAuthData(
                authType = authType,
                code = code,
                state = state
            ))
        }

        private fun sendContentMessage(msg: Any, engineSession: EngineSession) {
            val port = ports[engineSession]
            port?.postMessage(msg)
                ?: throw IllegalStateException("No port connected for provided session. Message not sent.")
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
    }
}
