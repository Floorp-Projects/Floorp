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
    )
) : WebExtension(id, url) {

    /**
     * See [WebExtension.registerBackgroundMessageHandler].
     */
    override fun registerBackgroundMessageHandler(name: String, messageHandler: MessageHandler) {
        val portDelegate = object : GeckoNativeWebExtension.PortDelegate {

            override fun onPortMessage(message: Any, port: GeckoNativeWebExtension.Port) {
                messageHandler.onPortMessage(message, GeckoPort(port))
            }

            override fun onDisconnect(port: GeckoNativeWebExtension.Port) {
                messageHandler.onPortDisconnected(GeckoPort(port))
            }
        }

        val messageDelegate = object : GeckoNativeWebExtension.MessageDelegate {

            override fun onConnect(port: GeckoNativeWebExtension.Port) {
                port.setDelegate(portDelegate)
                messageHandler.onPortConnected(GeckoPort(port))
            }

            override fun onMessage(message: Any, sender: GeckoNativeWebExtension.MessageSender): GeckoResult<Any>? {
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
                messageHandler.onPortDisconnected(GeckoPort(port, session))
            }
        }

        val messageDelegate = object : GeckoNativeWebExtension.MessageDelegate {

            override fun onConnect(port: GeckoNativeWebExtension.Port) {
                port.setDelegate(portDelegate)
                messageHandler.onPortConnected(GeckoPort(port, session))
            }

            override fun onMessage(message: Any, sender: GeckoNativeWebExtension.MessageSender): GeckoResult<Any>? {
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
}

/**
 * Gecko-based implementation of [Port], wrapping the native port provided by GeckoView.
 */
class GeckoPort(
    internal val nativePort: GeckoNativeWebExtension.Port,
    engineSession: EngineSession? = null
) : Port(engineSession) {

    override fun postMessage(message: Any) {
        nativePort.postMessage(message as JSONObject)
    }
}

private fun createWebExtensionFlags(allowContentMessaging: Boolean): Long {
    return if (allowContentMessaging) {
        GeckoNativeWebExtension.Flags.ALLOW_CONTENT_MESSAGING
    } else {
        GeckoNativeWebExtension.Flags.NONE
    }
}
