/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import mozilla.components.concept.engine.EngineSession
import org.json.JSONObject

/**
 * Represents a browser extension based on the WebExtension API:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions
 *
 * @property id the unique ID of this extension.
 * @property url the url pointing to a resources path for locating the extension
 * within the APK file e.g. resource://android/assets/extensions/my_web_ext.
 */
abstract class WebExtension(
    val id: String,
    val url: String
) {
    /**
     * Registers a [MessageHandler] for message events from background scripts.
     *
     * @param name the name of the native "application". This can either be the
     * name of an application, web extension or a specific feature in case
     * the web extension opens multiple [Port]s. There can only be one handler
     * with this name per extension and the same name has to be used in
     * JavaScript when calling `browser.runtime.connectNative` or
     * `browser.runtime.sendNativeMessage`. Note that name must match
     * /^\w+(\.\w+)*$/).
     * @param messageHandler the message handler to be notified of messaging
     * events e.g. a port was connected or a message received.
     */
    abstract fun registerBackgroundMessageHandler(name: String, messageHandler: MessageHandler)

    /**
     * Registers a [MessageHandler] for message events from content scripts.
     *
     * @param session the session to be observed / attach the message handler to.
     * @param name the name of the native "application". This can either be the
     * name of an application, web extension or a specific feature in case
     * the web extension opens multiple [Port]s. There can only be one handler
     * with this name per extension and session, and the same name has to be
     * used in JavaScript when calling `browser.runtime.connectNative` or
     * `browser.runtime.sendNativeMessage`. Note that name must match
     * /^\w+(\.\w+)*$/).
     * @param messageHandler the message handler to be notified of messaging
     * events e.g. a port was connected or a message received.
     */
    abstract fun registerContentMessageHandler(session: EngineSession, name: String, messageHandler: MessageHandler)

    /**
     * Checks whether there is an existing content message handler for the provided
     * session and "application" name.
     *
     * @param session the session the message handler was registered for.
     * @param name the "application" name the message handler was registered for.
     * @return true if a content message handler is active, otherwise false.
     */
    abstract fun hasContentMessageHandler(session: EngineSession, name: String): Boolean

    /**
     * Returns a connected port with the given name and for the provided
     * [EngineSession], if one exists.
     *
     * @param name the name as provided to connectNative.
     * @param session (optional) session to check for, null if port is from a
     * background script.
     * @return a matching port, or null if none is connected.
     */
    abstract fun getConnectedPort(name: String, session: EngineSession? = null): Port?

    /**
     * Disconnect a [Port] of the provided [EngineSession]. This method has
     * no effect if there's no connected port with the given name.
     *
     * @param name the name as provided to connectNative, see
     * [registerContentMessageHandler] and [registerBackgroundMessageHandler].
     * @param session (options) session for which ports should disconnected,
     * null if port is from a background script.
     */
    abstract fun disconnectPort(name: String, session: EngineSession? = null)
}

/**
 * A handler for all messaging related events, usable for both content and
 * background scripts.
 *
 * [Port]s are exposed to consumers (higher level components) because
 * how ports are used, how many there are and how messages map to it
 * is feature-specific and depends on the design of the web extension.
 * Therefore it makes most sense to let the extensions (higher-level
 * features) deal with the management of ports.
 */
interface MessageHandler {

    /**
     * Invoked when a [Port] was connected as a result of a
     * `browser.runtime.connectNative` call in JavaScript.
     *
     * @param port the connected port.
     */
    fun onPortConnected(port: Port) = Unit

    /**
     * Invoked when a [Port] was disconnected or the corresponding session was
     * destroyed.
     *
     * @param port the disconnected port.
     */
    fun onPortDisconnected(port: Port) = Unit

    /**
     * Invoked when a message was received on the provided port.
     *
     * @param message the received message, either be a primitive type
     * or a org.json.JSONObject.
     * @param port the port the message was received on.
     */
    fun onPortMessage(message: Any, port: Port) = Unit

    /**
     * Invoked when a message was received as a result of a
     * `browser.runtime.sendNativeMessage` call in JavaScript.
     *
     * @param message the received message, either be a primitive type
     * or a org.json.JSONObject.
     * @param source the session this message originated from if from a content
     * script, otherwise null.
     * @return the response to be sent for this message, either a primitive
     * type or a org.json.JSONObject, null if no response should be sent.
     */
    fun onMessage(message: Any, source: EngineSession?): Any? = Unit
}

/**
 * Represents a port for exchanging messages:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port
 */
abstract class Port(val engineSession: EngineSession? = null) {

    /**
     * Sends a message to this port.
     *
     * @param message the message to send.
     */
    abstract fun postMessage(message: JSONObject)

    /**
     * Returns the name of this port.
     */
    abstract fun name(): String

    /**
     * Disconnects this port.
     */
    abstract fun disconnect()
}
