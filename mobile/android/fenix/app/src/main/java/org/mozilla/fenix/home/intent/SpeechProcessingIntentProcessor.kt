/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import android.os.StrictMode
import androidx.navigation.NavController
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.widget.VoiceSearchActivity.Companion.SPEECH_PROCESSING

/**
 * The search widget has a microphone button to let users search with their voice.
 * Once the search is complete then a new search should be started.
 */
class SpeechProcessingIntentProcessor(
    private val activity: HomeActivity,
    private val metrics: MetricController
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (intent.extras?.getBoolean(HomeActivity.OPEN_TO_BROWSER_AND_LOAD) == true) {
            out.putExtra(HomeActivity.OPEN_TO_BROWSER_AND_LOAD, false)
            activity.components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                val searchEvent = MetricsUtils.createSearchEvent(
                    activity.components.search.provider.getDefaultEngine(activity),
                    activity,
                    Event.PerformedSearch.SearchAccessPoint.WIDGET
                )
                searchEvent?.let { metrics.track(it) }
            }

            activity.openToBrowserAndLoad(
                searchTermOrURL = intent.getStringExtra(SPEECH_PROCESSING).orEmpty(),
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
