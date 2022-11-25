/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import mozilla.components.concept.engine.EngineSession
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.openSetDefaultBrowserOption
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.DefaultBrowserNotificationWorker.Companion.isDefaultBrowserNotificationIntent
import org.mozilla.fenix.onboarding.ReEngagementNotificationWorker
import org.mozilla.fenix.onboarding.ReEngagementNotificationWorker.Companion.isReEngagementNotificationIntent

/**
 * When the default browser notification is tapped we need to launch [openSetDefaultBrowserOption]
 *
 * This should only happens once in a user's lifetime since once the user taps on the default browser
 * notification, [settings.shouldShowDefaultBrowserNotification] will return false
 */
class DefaultBrowserIntentProcessor(
    private val activity: HomeActivity,
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return when {
            isDefaultBrowserNotificationIntent(intent) -> {
                Events.defaultBrowserNotifTapped.record(NoExtras())

                activity.openSetDefaultBrowserOption()
                true
            }
            isReEngagementNotificationIntent(intent) -> {
                Events.reEngagementNotifTapped.record(NoExtras())

                activity.browsingModeManager.mode = BrowsingMode.Private
                activity.openToBrowserAndLoad(
                    ReEngagementNotificationWorker.NOTIFICATION_TARGET_URL,
                    newTab = true,
                    from = BrowserDirection.FromGlobal,
                    flags = EngineSession.LoadUrlFlags.external(),
                )
                true
            }
            else -> false
        }
    }
}
