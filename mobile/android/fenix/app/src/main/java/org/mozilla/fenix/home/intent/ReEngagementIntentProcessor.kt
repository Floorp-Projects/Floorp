/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import androidx.navigation.navOptions
import mozilla.components.concept.engine.EngineSession
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.onboarding.ReEngagementNotificationWorker
import org.mozilla.fenix.onboarding.ReEngagementNotificationWorker.Companion.isReEngagementNotificationIntent
import org.mozilla.fenix.utils.Settings

/**
 * Handle when the re-engagement notification is tapped
 *
 * This should only happens once in a user's lifetime notification,
 * [settings.shouldShowReEngagementNotification] will return false if the user already seen the
 * notification.
 */
class ReEngagementIntentProcessor(
    private val activity: HomeActivity,
    private val settings: Settings,
    private val appStore: AppStore,
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return when {
            isReEngagementNotificationIntent(intent) -> {
                Events.reEngagementNotifTapped.record(NoExtras())

                when (settings.reEngagementNotificationType) {
                    ReEngagementNotificationWorker.NOTIFICATION_TYPE_B -> {
                        val directions = NavGraphDirections.actionGlobalSearchDialog(sessionId = null)
                        val options = navOptions {
                            popUpTo(R.id.homeFragment)
                        }
                        navController.nav(null, directions, options)
                    }
                    else -> {
                        appStore.dispatch(AppAction.OpenTabInBrowser(BrowsingMode.Private))
                        activity.openToBrowserAndLoad(
                            ReEngagementNotificationWorker.NOTIFICATION_TARGET_URL,
                            newTab = true,
                            from = BrowserDirection.FromGlobal,
                            flags = EngineSession.LoadUrlFlags.external(),
                        )
                    }
                }

                true
            }
            else -> false
        }
    }
}
