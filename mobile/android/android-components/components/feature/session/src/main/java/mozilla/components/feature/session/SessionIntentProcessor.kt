/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Intent
import android.text.TextUtils
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.support.utils.SafeIntent

typealias IntentHandler = (Intent) -> Boolean

/**
 * Processor for intents which should trigger session-related actions.
 *
 * @param openNewTab Whether a processed Intent should open a new tab or open URLs in the currently
 *                   selected tab.
 */
class SessionIntentProcessor(
    private val sessionUseCases: SessionUseCases,
    private val sessionManager: SessionManager,
    useDefaultHandlers: Boolean = true,
    private val openNewTab: Boolean = true
) {
    private val defaultActionViewHandler = { intent: Intent ->
        val safeIntent = SafeIntent(intent)
        val url = safeIntent.dataString ?: ""

        when {
            TextUtils.isEmpty(url) -> false

            CustomTabConfig.isCustomTabIntent(safeIntent) -> {
                val session = Session(url).apply {
                    this.customTabConfig = CustomTabConfig.createFromIntent(safeIntent)
                }
                sessionManager.add(session)
                sessionUseCases.loadUrl.invoke(url, session)
                intent.putExtra(ACTIVE_SESSION_ID, session.id)
                true
            }

            else -> {
                val session = if (openNewTab) {
                    Session(url).also { sessionManager.add(it, selected = true) }
                } else {
                    sessionManager.selectedSession ?: Session(url)
                }

                sessionUseCases.loadUrl.invoke(url, session)

                true
            }
        }
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

    companion object {
        const val ACTIVE_SESSION_ID = "activeSessionId"
    }
}
