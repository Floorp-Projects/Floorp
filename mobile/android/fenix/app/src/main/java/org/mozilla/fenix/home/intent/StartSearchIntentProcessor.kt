/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.nav

/**
 * When the search widget is tapped, Fenix should open to the search fragment.
 */
class StartSearchIntentProcessor(
    private val metrics: MetricController
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (intent.extras?.getBoolean(HomeActivity.OPEN_TO_SEARCH) == true) {
            out.putExtra(HomeActivity.OPEN_TO_SEARCH, false)
            metrics.track(Event.SearchWidgetNewTabPressed)

            val directions = NavGraphDirections.actionGlobalSearch(sessionId = null, showShortcutEnginePicker = true)
            navController.nav(null, directions)
            true
        } else {
            false
        }
    }
}
