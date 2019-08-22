/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import mozilla.components.lib.crash.Crash
import org.mozilla.fenix.NavGraphDirections

/**
 * When the app crashes, the user has the option to report it.
 * Reporting fires an intent to the main activity which is handled here.
 */
class CrashReporterIntentProcessor : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (Crash.isCrashIntent(intent)) {
            openToCrashReporter(intent, navController)
            true
        } else {
            false
        }
    }

    private fun openToCrashReporter(intent: Intent, navController: NavController) {
        val directions = NavGraphDirections.actionGlobalCrashReporter(intent)
        navController.navigate(directions)
    }
}
