/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import org.json.JSONObject

/**
 * [MessageHandler] implementation that receives messages from the icons web extensions and performs icon loads.
 */
internal class IconMessageHandler(
    private val store: BrowserStore,
    private val sessionId: String,
    private val private: Boolean,
    private val icons: BrowserIcons,
) : MessageHandler {
    private val scope = CoroutineScope(Dispatchers.IO)

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE) // This only exists so that we can wait in tests.
    internal var lastJob: Job? = null

    override fun onMessage(message: Any, source: EngineSession?): Any {
        if (message is JSONObject) {
            message.toIconRequest(private)?.let { loadRequest(it) }
        } else {
            throw IllegalStateException("Received unexpected message: $message")
        }

        // Needs to return something that is not null and not Unit:
        // https://github.com/mozilla-mobile/android-components/issues/2969
        return ""
    }

    private fun loadRequest(request: IconRequest) {
        lastJob = scope.launch {
            val icon = icons.loadIcon(request).await()

            store.dispatch(ContentAction.UpdateIconAction(sessionId, request.url, icon.bitmap))
        }
    }
}
