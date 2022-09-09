/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import android.content.Intent.ACTION_VIEW
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.intent.ext.putSessionId
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.utils.toSafeIntent

/**
 * Legacy processor for pinned websites shortcuts created by Fennec.
 * https://developer.android.com/guide/topics/ui/shortcuts/creating-shortcuts#pinned
 */
class FennecBookmarkShortcutsIntentProcessor(
    private val addNewTabUseCase: TabsUseCases.AddNewTabUseCase,
) : IntentProcessor {

    /**
     * Returns true if this Intent is of a Fennec pinned shortcut.
     */
    private fun matches(intent: Intent): Boolean {
        return intent.toSafeIntent().action == ACTION_FENNEC_HOMESCREEN_SHORTCUT
    }

    /**
     * If this is an Intent for a Fennec pinned website shortcut
     * prepare it for opening website's URL in a new tab.
     */
    override fun process(intent: Intent): Boolean {
        val safeIntent = intent.toSafeIntent()
        val url = safeIntent.dataString

        return if (!url.isNullOrEmpty() && matches(intent)) {
            val sessionId = addNewTabUseCase(
                url = url,
                flags = EngineSession.LoadUrlFlags.external(),
                source = SessionState.Source.Internal.HomeScreen,
                selectTab = true,
                startLoading = true,
            )
            intent.action = ACTION_VIEW
            intent.putSessionId(sessionId)
            true
        } else {
            false
        }
    }

    @VisibleForTesting
    companion object {
        /**
         * Fennec set action for the pinned website shortcut Intent.
         */
        const val ACTION_FENNEC_HOMESCREEN_SHORTCUT = "org.mozilla.gecko.BOOKMARK"
    }
}
