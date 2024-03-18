/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController

/**
 * Processor for Android intents received in [org.mozilla.fenix.HomeActivity].
 */
interface HomeIntentProcessor {

    /**
     * Processes the given [intent]. May add properties to [out].
     *
     * @param intent The intent to process.
     * @param navController Controller to navigate between fragments.
     * @param out Intent to mutate.
     * @return True if the intent was processed, otherwise false.
     */
    fun process(intent: Intent, navController: NavController, out: Intent): Boolean
}
