/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject
import java.util.concurrent.ConcurrentHashMap

/**
 * Provides functionality to feature modules that need to interact with a web extension.
 *
 * @property extensionId the unique ID of the web extension e.g. mozacReaderview.
 * @property extensionUrl the url pointing to a resources path for locating the
 * extension within the APK file e.g. resource://android/assets/extensions/my_web_ext.
 */
class WebExtensionController(
    private val extensionId: String,
    private val extensionUrl: String
) {
    private val logger = Logger("mozac-webextensions")
    private var registerContentMessageHandler: (WebExtension) -> Unit? = { }

    /**
     * Makes sure the web extension is installed in the provided engine. If a
     * content message handler was registered (see
     * [registerContentMessageHandler]) before install completed, registration
     * will happen upon successful installation.
     *
     * @param engine the [Engine] the web extension should be installed in.
     */
    fun install(engine: Engine) {
        if (!installedExtensions.containsKey(extensionId)) {
            engine.installWebExtension(extensionId, extensionUrl,
                onSuccess = {
                    logger.debug("Installed extension: ${it.id}")
                    synchronized(this@WebExtensionController) {
                        registerContentMessageHandler(it)
                        installedExtensions[extensionId] = it
                    }
                },
                onError = { ext, throwable ->
                    logger.error("Failed to install extension: $ext", throwable)
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
     * @param name (optional) name of the port, defaults to the provided extensionId.
     */
    fun registerContentMessageHandler(
        engineSession: EngineSession,
        messageHandler: MessageHandler,
        name: String = extensionId
    ) {
        synchronized(this) {
            registerContentMessageHandler = {
                it.registerContentMessageHandler(engineSession, name, messageHandler)
            }

            installedExtensions[extensionId]?.let { registerContentMessageHandler(it) }
        }
    }

    /**
     * Sends a content message to the provided session.
     *
     * @param msg the message to send
     * @param engineSession the session to send the content message to.
     * @param name (optional) name of the port, defaults to the provided extensionId.
     */
    fun sendContentMessage(msg: JSONObject, engineSession: EngineSession?, name: String = extensionId) {
        engineSession?.let { session ->
            installedExtensions[extensionId]?.let { ext ->
                val port = ext.getConnectedPort(name, session)
                port?.postMessage(msg) ?: logger.error("No port connected for provided session. Message $msg not sent.")
            }
        }
    }

    /**
     * Checks whether or not a port is connected for the provided session.
     *
     * @param engineSession the session the port should be connected to.
     * @param name (optional) name of the port, defaults to the provided extensionId.
     */
    fun portConnected(engineSession: EngineSession?, name: String = extensionId): Boolean {
        return engineSession?.let { session ->
            installedExtensions[extensionId]?.let { ext ->
                ext.getConnectedPort(name, session) != null
            }
        } ?: false
    }

    /**
     * Disconnects the port of the provided session.
     *
     * @param engineSession the session the port is connected to.
     * @param name (optional) name of the port, defaults to the provided extensionId.
     */
    fun disconnectPort(engineSession: EngineSession?, name: String = extensionId) {
        engineSession?.let { session ->
            installedExtensions[extensionId]?.let { ext ->
                ext.disconnectPort(name, session)
            }
        }
    }

    companion object {
        @VisibleForTesting
        val installedExtensions = ConcurrentHashMap<String, WebExtension>()
    }
}
