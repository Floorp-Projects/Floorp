/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.openSetDefaultBrowserOption
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.DefaultBrowserNotificationWorker.Companion.isDefaultBrowserNotificationIntent

/**
 * When the default browser notification is tapped we need to launch [openSetDefaultBrowserOption]
 *
 * This should only happens once in a user's lifetime since once the user taps on the default browser
 * notification, [settings.shouldShowDefaultBrowserNotification] will return false
 */
class DefaultBrowserIntentProcessor(
    private val activity: HomeActivity,
    private val metrics: MetricController
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (isDefaultBrowserNotificationIntent(intent)) {
            activity.openSetDefaultBrowserOption()
            metrics.track(Event.DefaultBrowserNotifTapped)
            true
        } else {
            false
        }
    }
}
