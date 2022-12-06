/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import androidx.navigation.navOptions
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.ext.nav

/**
 * Long pressing home button should also open to the search fragment if Fenix is set as the
 * assist app
 */
class AssistIntentProcessor : HomeIntentProcessor {
    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        if (intent.action != Intent.ACTION_ASSIST) {
            return false
        }

        val directions = NavGraphDirections.actionGlobalSearchDialog(
            sessionId = null,
            // Will follow this up with adding `ASSIST` as a search source.
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1808043
            searchAccessPoint = MetricsUtils.Source.NONE,
        )

        val options = navOptions {
            popUpTo(R.id.homeFragment)
        }

        navController.nav(null, directions, options)

        return true
    }
}
