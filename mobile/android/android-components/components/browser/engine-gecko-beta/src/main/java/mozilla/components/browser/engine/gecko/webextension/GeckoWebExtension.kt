/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import org.json.JSONObject
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.WebExtension as GeckoNativeWebExtension

/**
 * Gecko-based implementation of [WebExtension], wrapping the native web
 * extension object provided by GeckoView.
 */
class GeckoWebExtension(
    id: String,
    url: String,
    allowContentMessaging: Boolean = true,
    val nativeExtension: GeckoNativeWebExtension = GeckoNativeWebExtension(
        url,
        id,
        createWebExtensionFlags(allowContentMessaging)
    ),
    private val connectedPorts: MutableMap<PortId, Port> = mutableMapOf()
) : WebExtension(id, url) {

    /**
     * Uniquely identifies a port using its name and the session it
     * was opened for. Ports connected from background scripts will
     * have a null session.
     */
    data class PortId(val name: String, val session: EngineSession? = null)

    /**
     * See [WebExtension.registerBackgroundMessageHandler].
     */
    override fun registerBackgroundMessageHandler(name: String, messageHandler: MessageHandler) {
        val portDelegate = object : GeckoNativeWebExtension.PortDelegate {

            override fun onPortMessage(message: Any, port: GeckoNativeWebExtension.Port) {
                messageHandler.onPortMessage(message, GeckoPort(port))
            }

            override fun onDisconnect(port: GeckoNativeWebExtension.Port) {
                connectedPorts.remove(PortId(name))
                messageHandler.onPortDisconnected(GeckoPort(port))
            }
        }

        val messageDelegate = object : GeckoNativeWebExtension.MessageDelegate {

            override fun onConnect(port: GeckoNativeWebExtension.Port) {
                port.setDelegate(portDelegate)
                val geckoPort = GeckoPort(port)
                connectedPorts[PortId(name)] = geckoPort
                messageHandler.onPortConnected(geckoPort)
            }

            override fun onMessage(
                // We don't use the same delegate instance for multiple apps so we don't need to verify the name.
                name: String,
                message: Any,
                sender: GeckoNativeWebExtension.MessageSender
            ): GeckoResult<Any>? {
                val response = messageHandler.onMessage(message, null)
                return response?.let { GeckoResult.fromValue(it) }
            }
        }

        nativeExtension.setMessageDelegate(messageDelegate, name)
    }

    /**
     * See [WebExtension.registerContentMessageHandler].
     */
    override fun registerContentMessageHandler(session: EngineSession, name: String, messageHandler: MessageHandler) {
        val portDelegate = object : GeckoNativeWebExtension.PortDelegate {

            override fun onPortMessage(message: Any, port: GeckoNativeWebExtension.Port) {
                messageHandler.onPortMessage(message, GeckoPort(port, session))
            }

            override fun onDisconnect(port: GeckoNativeWebExtension.Port) {
                val geckoPort = GeckoPort(port, session)
                connectedPorts.remove(PortId(name, session))
                messageHandler.onPortDisconnected(geckoPort)
            }
        }

        val messageDelegate = object : GeckoNativeWebExtension.MessageDelegate {

            override fun onConnect(port: GeckoNativeWebExtension.Port) {
                port.setDelegate(portDelegate)
                val geckoPort = GeckoPort(port, session)
                connectedPorts[PortId(name, session)] = geckoPort
                messageHandler.onPortConnected(geckoPort)
            }

            override fun onMessage(
                // We don't use the same delegate instance for multiple apps so we don't need to verify the name.
                name: String,
                message: Any,
                sender: GeckoNativeWebExtension.MessageSender
            ): GeckoResult<Any>? {
                val response = messageHandler.onMessage(message, session)
                return response?.let { GeckoResult.fromValue(it) }
            }
        }

        val geckoSession = (session as GeckoEngineSession).geckoSession
        geckoSession.setMessageDelegate(nativeExtension, messageDelegate, name)
    }

    /**
     * See [WebExtension.hasContentMessageHandler].
     */
    override fun hasContentMessageHandler(session: EngineSession, name: String): Boolean {
        val geckoSession = (session as GeckoEngineSession).geckoSession
        return geckoSession.getMessageDelegate(nativeExtension, name) != null
    }

    /**
     * See [WebExtension.getConnectedPort].
     */
    override fun getConnectedPort(name: String, session: EngineSession?): Port? {
        return connectedPorts[PortId(name, session)]
    }

    /**
     * See [WebExtension.disconnectPort].
     */
    override fun disconnectPort(name: String, session: EngineSession?) {
        val portId = PortId(name, session)
        val port = connectedPorts[portId]
        port?.let {
            it.disconnect()
            connectedPorts.remove(portId)
        }
    }
}

/**
 * Gecko-based implementation of [Port], wrapping the native port provided by GeckoView.
 */
class GeckoPort(
    internal val nativePort: GeckoNativeWebExtension.Port,
    engineSession: EngineSession? = null
) : Port(engineSession) {

    override fun postMessage(message: JSONObject) {
        nativePort.postMessage(message)
    }

    override fun name(): String {
        return nativePort.name
    }

    override fun disconnect() {
        nativePort.disconnect()
    }
}

private fun createWebExtensionFlags(allowContentMessaging: Boolean): Long {
    return if (allowContentMessaging) {
        GeckoNativeWebExtension.Flags.ALLOW_CONTENT_MESSAGING
    } else {
        GeckoNativeWebExtension.Flags.NONE
    }
}
