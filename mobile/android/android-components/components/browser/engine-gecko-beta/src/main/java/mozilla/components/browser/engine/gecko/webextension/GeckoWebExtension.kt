package mozilla.components.browser.engine.gecko.webextension

import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.WebExtension
import org.mozilla.geckoview.WebExtension as GeckoNativeWebExtension

/**
 * Gecko-based implementation of [WebExtension], wrapping the native web
 * extension object provided by GeckoView.
 */
class GeckoWebExtension(
    id: String,
    url: String,
    val nativeExtension: GeckoNativeWebExtension = GeckoNativeWebExtension(url, id)
) : WebExtension(id, url) {

    // Not supported in beta
    override fun registerContentMessageHandler(
        session: EngineSession,
        name: String,
        messageHandler: MessageHandler
    ) = Unit

    // Not supported in beta
    override fun registerBackgroundMessageHandler(name: String, messageHandler: MessageHandler) = Unit

    // Not supported in beta
    override fun hasContentMessageHandler(session: EngineSession, name: String): Boolean = false
}
