/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent

import android.content.Context
import android.content.Intent
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.intent.EXTRA_SESSION_ID
import mozilla.components.feature.customtabs.CustomTabIntentProcessor
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases

typealias IntentHandler = (Intent) -> Boolean

/**
 * Processor for intents which should trigger session-related actions.
 *
 * @property sessionUseCases A reference to [SessionUseCases] used to load URLs.
 * @property sessionManager The application's [SessionManager].
 * @property searchUseCases A reference to [SearchUseCases] to be used for ACTION_SEND
 * intents if the provided text is not a URL.
 * @property useDefaultHandlers Whether or not the built-in handlers should be used.
 * @property openNewTab Whether a processed intent should open a new tab or
 * open URLs in the currently selected tab.
 * @property isPrivate Whether a processed intent should open a new tab as private
 *
 * @deprecated Use individual intent processors instead.
 */
@Deprecated("Use individual processors such as TabIntentProcessor instead.")
class IntentProcessor(
    private val sessionUseCases: SessionUseCases,
    private val sessionManager: SessionManager,
    private val searchUseCases: SearchUseCases,
    private val context: Context,
    private val useDefaultHandlers: Boolean = true,
    private val openNewTab: Boolean = true,
    private val isPrivate: Boolean = false
) {

    private val defaultHandlers by lazy {
        val tabIntentProcessor = TabIntentProcessor(
            sessionManager,
            sessionUseCases.loadUrl,
            if (isPrivate) searchUseCases.newPrivateTabSearch else searchUseCases.newTabSearch,
            openNewTab,
            isPrivate
        )
        val customTabIntentProcessor = CustomTabIntentProcessor(
            sessionManager,
            sessionUseCases.loadUrl,
            context.resources
        )
        val viewHandlers = listOf(customTabIntentProcessor, tabIntentProcessor)

        mutableMapOf<String, IntentHandler>(
            Intent.ACTION_VIEW to { intent ->
                runBlocking { viewHandlers.any { it.process(intent) } }
            },
            Intent.ACTION_SEND to { intent ->
                runBlocking { tabIntentProcessor.process(intent) }
            }
        )
    }

    private val handlers = if (useDefaultHandlers) defaultHandlers else mutableMapOf()

    /**
     * Processes the given intent by invoking the registered handler.
     *
     * @param intent the intent to process
     * @return true if the intent was processed, otherwise false.
     */
    fun process(intent: Intent): Boolean {
        val action = intent.action ?: return false
        return handlers[action]?.invoke(intent) ?: false
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

    companion object {
        const val ACTIVE_SESSION_ID = EXTRA_SESSION_ID
    }
}
