/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent

import android.content.Context
import android.content.Intent
import android.text.TextUtils
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.utils.SafeIntent

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
 */
class IntentProcessor(
    private val sessionUseCases: SessionUseCases,
    private val sessionManager: SessionManager,
    private val searchUseCases: SearchUseCases,
    private val context: Context,
    private val useDefaultHandlers: Boolean = true,
    private val openNewTab: Boolean = true
) {
    private val defaultActionViewHandler = { intent: Intent ->
        val safeIntent = SafeIntent(intent)
        val url = safeIntent.dataString ?: ""

        when {
            TextUtils.isEmpty(url) -> false

            CustomTabConfig.isCustomTabIntent(safeIntent) -> {
                val session = Session(url, false, Source.CUSTOM_TAB).apply {
                    val displayMetrics = context.resources.displayMetrics
                    this.customTabConfig = CustomTabConfig.createFromIntent(safeIntent, displayMetrics)
                }
                sessionManager.add(session)
                sessionUseCases.loadUrl.invoke(url, session)
                intent.putExtra(ACTIVE_SESSION_ID, session.id)
                true
            }

            else -> {
                val session = createSession(url, source = Source.ACTION_VIEW)
                sessionUseCases.loadUrl.invoke(url, session)
                true
            }
        }
    }

    private val defaultActionSendHandler = { intent: Intent ->
        val safeIntent = SafeIntent(intent)
        val extraText = safeIntent.getStringExtra(Intent.EXTRA_TEXT) ?: ""

        when {
            TextUtils.isEmpty(extraText.trim()) -> false

            else -> {
                val url = extraText.split(" ").find { it.isUrl() }
                if (url != null) {
                    val session = createSession(url, source = Source.ACTION_SEND)
                    sessionUseCases.loadUrl.invoke(url, session)
                    true
                } else {
                    searchUseCases.newTabSearch.invoke(extraText, Source.ACTION_SEND, openNewTab)
                    true
                }
            }
        }
    }

    private fun createSession(url: String, private: Boolean = false, source: Source): Session {
        return if (openNewTab) {
            Session(url, private, source).also { sessionManager.add(it, selected = true) }
        } else {
            sessionManager.selectedSession ?: Session(url, private, source)
        }
    }

    private val defaultHandlers: MutableMap<String, IntentHandler> by lazy {
        mutableMapOf(
            Intent.ACTION_VIEW to defaultActionViewHandler,
            Intent.ACTION_SEND to defaultActionSendHandler
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
