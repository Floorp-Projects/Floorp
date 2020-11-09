/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.sessionsOfType

/**
 * The Private Browsing Mode notification has an "Delete and Open" button to let users delete all
 * of their private tabs.
 */
class NotificationsIntentProcessor(
    private val activity: HomeActivity
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (intent.extras?.getBoolean(HomeActivity.EXTRA_DELETE_PRIVATE_TABS) == true) {
            out.putExtra(HomeActivity.EXTRA_DELETE_PRIVATE_TABS, false)
            activity.components.core.sessionManager.run {
                sessionsOfType(private = true).forEach { remove(it) }
            }
            true
        } else intent.extras?.getBoolean(HomeActivity.EXTRA_OPENED_FROM_NOTIFICATION) == true
    }
}
