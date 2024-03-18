/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shortcut

import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_CLEAR_TASK
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor

class NewTabShortcutIntentProcessor : IntentProcessor {

    /**
     * Processes the given [Intent].
     *
     * @param intent The intent to process.
     * @return True if the intent was processed, otherwise false.
     */
    override fun process(intent: Intent): Boolean {
        val safeIntent = SafeIntent(intent)
        val (searchExtra, startPrivateMode) = when (safeIntent.action) {
            ACTION_OPEN_TAB -> StartSearchIntentProcessor.STATIC_SHORTCUT_NEW_TAB to false
            ACTION_OPEN_PRIVATE_TAB -> StartSearchIntentProcessor.STATIC_SHORTCUT_NEW_PRIVATE_TAB to true
            else -> return false
        }

        intent.putExtra(HomeActivity.OPEN_TO_SEARCH, searchExtra)
        intent.putExtra(HomeActivity.PRIVATE_BROWSING_MODE, startPrivateMode)
        intent.flags = intent.flags or FLAG_ACTIVITY_NEW_TASK or FLAG_ACTIVITY_CLEAR_TASK

        return true
    }

    companion object {
        const val ACTION_OPEN_TAB = "org.mozilla.fenix.OPEN_TAB"
        const val ACTION_OPEN_PRIVATE_TAB = "org.mozilla.fenix.OPEN_PRIVATE_TAB"
    }
}
