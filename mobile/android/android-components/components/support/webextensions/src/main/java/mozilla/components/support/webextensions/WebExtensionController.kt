/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject
import java.util.concurrent.ConcurrentHashMap

/**
 * Provides functionality to feature modules that need to interact with a web extension.
 *
 * @property extensionId the unique ID of the web extension e.g. mozacReaderview.
 * @property extensionUrl the url pointing to a resources path for locating the
 * extension within the APK file e.g. resource://android/assets/extensions/my_web_ext.
 * @property messagingId the unique ID used to exchange messages between extension
 * scripts and the application.
 */
class WebExtensionController(
    private val extensionId: String,
    private val extensionUrl: String,
    private val messagingId: String
) {
    private val logger = Logger("mozac-webextensions")
    private var registerContentMessageHandler: (WebExtension) -> Unit? = { }
    private var registerBackgroundMessageHandler: (WebExtension) -> Unit? = { }

    /**
     * Makes sure the web extension is installed in the provided runtime. If a
     * content message handler was registered (see
     * [registerContentMessageHandler]) before install completed, registration
     * will happen upon successful installation.
     *
     * @param runtime the [WebExtensionRuntime] the web extension should be installed in.
     * @param onSuccess (optional) callback invoked if the extension was installed successfully,
     * providing access to the [WebExtension] object.
     * @param onError (optional) callback invoked if there was an error installing the extension.
     */
    fun install(
        runtime: WebExtensionRuntime,
        onSuccess: ((WebExtension) -> Unit) = { },
        onError: ((Throwable) -> Unit) = { _ -> }
    ) {
        if (!installedExtensions.containsKey(extensionId)) {
            runtime.installWebExtension(extensionId, extensionUrl,
                onSuccess = {
                    logger.debug("Installed extension: ${it.id}")
                    synchronized(this@WebExtensionController) {
                        registerContentMessageHandler(it)
                        registerBackgroundMessageHandler(it)
                        installedExtensions[extensionId] = it
                        onSuccess(it)
                    }
                },
                onError = { ext, throwable ->
                    logger.error("Failed to install extension: $ext", throwable)
                    onError(throwable)
                }
            )
        }
    }

    /**
     * Registers a content message handler for the provided session. Currently only one
     * handler can be registered per session. An existing handler will be replaced and
     * there is no need to unregister.
     *
     * @param engineSession the session the content message handler should be registered with.
     * @param messageHandler the message handler to register.
     * @param name (optional) name of the port, defaults to the provided messagingId.
     */
    fun registerContentMessageHandler(
        engineSession: EngineSession,
        messageHandler: MessageHandler,
        name: String = messagingId
    ) {
        synchronized(this) {
            registerContentMessageHandler = {
                it.registerContentMessageHandler(engineSession, name, messageHandler)
            }

            installedExtensions[extensionId]?.let { registerContentMessageHandler(it) }
        }
    }

    /**
     * Registers a background message handler for this extension. An existing handler
     * will be replaced and there is no need to unregister.
     *
     * @param messageHandler the message handler to register.
     * @param name (optional) name of the port, defaults to the provided messagingId.
     * */
    fun registerBackgroundMessageHandler(
        messageHandler: MessageHandler,
        name: String = messagingId
    ) {
        synchronized(this) {
            registerBackgroundMessageHandler = {
                it.registerBackgroundMessageHandler(name, messageHandler)
            }

            installedExtensions[extensionId]?.let { registerBackgroundMessageHandler(it) }
        }
    }

    /**
     * Sends a content message to the provided session.
     *
     * @param msg the message to send
     * @param engineSession the session to send the content message to.
     * @param name (optional) name of the port, defaults to the provided [messagingId].
     */
    fun sendContentMessage(msg: JSONObject, engineSession: EngineSession?, name: String = messagingId) {
        engineSession?.let { session ->
            installedExtensions[extensionId]?.let { ext ->
                val port = ext.getConnectedPort(name, session)
                port?.postMessage(msg)
                    ?: logger.error("No port with name $name connected for provided session. Message $msg not sent.")
            }
        }
    }

    /**
     * Sends a background message to the provided extension.
     *
     * @param msg the message to send
     * @param name (optional) name of the port, defaults to the provided [messagingId].
     */
    fun sendBackgroundMessage(
        msg: JSONObject,
        name: String = messagingId
    ) {
        installedExtensions[extensionId]?.let { ext ->
            val port = ext.getConnectedPort(name)
            port?.postMessage(msg)
                    ?: logger.error("No port connected for provided extension. Message $msg not sent.")
        }
    }

    /**
     * Checks whether or not a port is connected for the provided session.
     *
     * @param engineSession the session the port should be connected to or null for a port to a background script.
     * @param name (optional) name of the port, defaults to the provided [messagingId].
     */
    fun portConnected(engineSession: EngineSession?, name: String = messagingId): Boolean {
        return installedExtensions[extensionId]?.let { ext ->
            ext.getConnectedPort(name, engineSession) != null
        } ?: false
    }

    /**
     * Disconnects the port of the provided session.
     *
     * @param engineSession the session the port is connected to or null for a port to a background script.
     * @param name (optional) name of the port, defaults to the provided [messagingId].
     */
    fun disconnectPort(engineSession: EngineSession?, name: String = messagingId) {
        installedExtensions[extensionId]?.disconnectPort(name, engineSession)
    }

    companion object {
        @VisibleForTesting
        val installedExtensions = ConcurrentHashMap<String, WebExtension>()
    }
}
