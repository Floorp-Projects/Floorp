/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.intent.processing

import android.app.SearchManager
import android.content.Intent
import android.content.Intent.ACTION_MAIN
import android.content.Intent.ACTION_SEARCH
import android.content.Intent.ACTION_SEND
import android.content.Intent.ACTION_VIEW
import android.content.Intent.ACTION_WEB_SEARCH
import android.content.Intent.EXTRA_TEXT
import android.nfc.NfcAdapter.ACTION_NDEF_DISCOVERED
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.externalPackage
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.WebURLFinder

/**
 * Processor for intents which should trigger session-related actions.
 *
 * @property tabsUseCases An instance of [TabsUseCases] used to open new tabs.
 * @property newTabSearchUseCase A reference to [SearchUseCases.NewTabSearchUseCase] to be used for
 * ACTION_SEND intents if the provided text is not a URL.
 * @property isPrivate Whether a processed intent should open a new tab as private
 */
class TabIntentProcessor(
    private val tabsUseCases: TabsUseCases,
    private val newTabSearchUseCase: SearchUseCases.NewTabSearchUseCase,
    private val isPrivate: Boolean = false,
) : IntentProcessor {

    /**
     * Loads a URL from a view intent in a new session.
     */
    private fun processViewIntent(intent: SafeIntent): Boolean {
        val url = intent.dataString

        return if (url.isNullOrEmpty()) {
            false
        } else {
            val caller = intent.externalPackage()
            tabsUseCases.selectOrAddTab(
                url.toNormalizedUrl(),
                private = isPrivate,
                source = SessionState.Source.External.ActionView(caller),
                flags = LoadUrlFlags.external(),
            )
            true
        }
    }

    /**
     * Processes a send intent and tries to load [EXTRA_TEXT] as a URL.
     * If it's not a URL, a search is run instead.
     */
    private fun processSendIntent(intent: SafeIntent): Boolean {
        val extraText = intent.getStringExtra(EXTRA_TEXT)

        return if (extraText.isNullOrBlank()) {
            false
        } else {
            val url = WebURLFinder(extraText).bestWebURL()
            val source = SessionState.Source.External.ActionSend(intent.externalPackage())
            if (url != null) {
                addNewTab(url, source)
            } else {
                newTabSearchUseCase(extraText, source)
            }
            true
        }
    }

    private fun processSearchIntent(intent: SafeIntent): Boolean {
        val searchQuery = intent.getStringExtra(SearchManager.QUERY)

        return if (searchQuery.isNullOrBlank()) {
            false
        } else {
            val source = SessionState.Source.External.ActionSearch(intent.externalPackage())
            if (searchQuery.isUrl()) {
                addNewTab(searchQuery, source)
            } else {
                newTabSearchUseCase(searchQuery, source)
            }
            true
        }
    }

    private fun addNewTab(url: String, source: SessionState.Source) {
        tabsUseCases.addTab(
            url.toNormalizedUrl(),
            source = source,
            flags = LoadUrlFlags.external(),
            private = isPrivate,
        )
    }

    /**
     * Processes the given intent by invoking the registered handler.
     *
     * @param intent the intent to process
     * @return true if the intent was processed, otherwise false.
     */
    override fun process(intent: Intent): Boolean {
        val safeIntent = SafeIntent(intent)
        return when (safeIntent.action) {
            ACTION_VIEW, ACTION_MAIN, ACTION_NDEF_DISCOVERED -> processViewIntent(safeIntent)
            ACTION_SEND -> processSendIntent(safeIntent)
            ACTION_SEARCH, ACTION_WEB_SEARCH -> processSearchIntent(safeIntent)
            else -> false
        }
    }
}
