/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity

/**
 * The search widget has a microphone button to let users search with their voice.
 * Once the search is complete then a new search should be started.
 */
class SpeechProcessingIntentProcessor(
    private val activity: HomeActivity
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (intent.extras?.getBoolean(HomeActivity.OPEN_TO_BROWSER_AND_LOAD) == true) {
            out.putExtra(HomeActivity.OPEN_TO_BROWSER_AND_LOAD, false)
            activity.openToBrowserAndLoad(
                searchTermOrURL = intent.getStringExtra(IntentReceiverActivity.SPEECH_PROCESSING).orEmpty(),
                newTab = true,
                from = BrowserDirection.FromGlobal,
                forceSearch = true
            )
            true
        } else {
            false
        }
    }
}
