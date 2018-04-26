/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Intent
import android.text.TextUtils

typealias IntentHandler = (Intent) -> Boolean

/**
 * Processor for intents which should trigger session-related actions.
 */
class SessionIntentProcessor(
    private val sessionUseCases: SessionUseCases,
    useDefaultHandlers: Boolean = true
) {
    private val defaultActionViewHandler = { intent: Intent ->
        val url = intent.dataString
        if (TextUtils.isEmpty(url)) {
            false
        }

        // TODO switch to loadUrlInNewTab, once available
        sessionUseCases.loadUrl.invoke(url)
        true
    }

    private val defaultHandlers: MutableMap<String, IntentHandler> by lazy {
        mutableMapOf(Intent.ACTION_VIEW to defaultActionViewHandler)
    }

    private val handlers = if (useDefaultHandlers) defaultHandlers else mutableMapOf()

    /**
     * Processes the given intent by invoking the registered handler.
     *
     * @param intent the intent to process
     * @return true if the intent was processed, otherwise false.
     */
    fun process(intent: Intent): Boolean {
        return handlers[intent.action]?.invoke(intent) ?: false
    }

    /**
     * Registers the given handler to be invoked for intents with the given action. If a
     * handler is already present it will be overwritten.
     *
     * @param action the intent action which should trigger the provided handler.
     * @param handler the intent handler to be registered.
     */
    fun registerHandler(action: String, handler: IntentHandler) {
        handlers[action] = handler
    }

    /**
     * Removes the registered handler for the given intent action, if present.
     *
     * @param action the intent action for which the handler should be removed.
     */
    fun unregisterHandler(action: String) {
        handlers.remove(action)
    }
}
