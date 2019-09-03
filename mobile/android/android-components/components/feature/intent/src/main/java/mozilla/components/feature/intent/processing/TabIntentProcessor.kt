/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent.processing

import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.ACTION_VIEW
import android.content.Intent.EXTRA_TEXT
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.WebURLFinder

/**
 * Processor for intents which should trigger session-related actions.
 *
 * @property sessionManager The application's [SessionManager].
 * @property loadUrlUseCase A reference to [SessionUseCases.DefaultLoadUrlUseCase] used to load URLs.
 * @property newTabSearchUseCase A reference to [SearchUseCases.NewTabSearchUseCase] to be used for
 * ACTION_SEND intents if the provided text is not a URL.
 * @property openNewTab Whether a processed intent should open a new tab or
 * open URLs in the currently selected tab.
 * @property isPrivate Whether a processed intent should open a new tab as private
 */
class TabIntentProcessor(
    private val sessionManager: SessionManager,
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase,
    private val newTabSearchUseCase: SearchUseCases.NewTabSearchUseCase,
    private val openNewTab: Boolean = true,
    private val isPrivate: Boolean = false
) : IntentProcessor {

    /**
     * Loads a URL from a view intent in a new session.
     */
    private fun processViewIntent(intent: SafeIntent): Boolean {
        val url = intent.dataString

        return if (url.isNullOrEmpty()) {
            false
        } else {
            val session = createSession(url, private = isPrivate, source = Source.ACTION_VIEW)
            loadUrlUseCase(url, session, LoadUrlFlags.external())
            true
        }
    }

    /**
     * Processes a send intent and tries to load [EXTRA_TEXT] as a URL.
     * If its not a URL, a search is run instead.
     */
    private fun processSendIntent(intent: SafeIntent): Boolean {
        val extraText = intent.getStringExtra(EXTRA_TEXT)

        return if (extraText.isNullOrBlank()) {
            false
        } else {
            val url = WebURLFinder(extraText).bestWebURL()
            if (url != null) {
                val session = createSession(url, private = isPrivate, source = Source.ACTION_SEND)
                loadUrlUseCase(url, session, LoadUrlFlags.external())
            } else {
                newTabSearchUseCase(extraText, Source.ACTION_SEND, openNewTab)
            }
            true
        }
    }

    private fun createSession(url: String, private: Boolean = false, source: Source): Session {
        return if (openNewTab) {
            Session(url, private, source).also { sessionManager.add(it, selected = true) }
        } else {
            sessionManager.selectedSession ?: Session(url, private, source)
        }
    }

    override fun matches(intent: Intent): Boolean {
        val safeIntent = SafeIntent(intent)
        return safeIntent.action == ACTION_VIEW || safeIntent.action == ACTION_SEND
    }

    /**
     * Processes the given intent by invoking the registered handler.
     *
     * @param intent the intent to process
     * @return true if the intent was processed, otherwise false.
     */
    override suspend fun process(intent: Intent): Boolean {
        val safeIntent = SafeIntent(intent)
        return when (safeIntent.action) {
            ACTION_VIEW -> processViewIntent(safeIntent)
            ACTION_SEND -> processSendIntent(safeIntent)
            else -> false
        }
    }
}
