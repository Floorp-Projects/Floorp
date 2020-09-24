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
 * Tapping the private browsing mode launcher icon should also open to the search fragment.
 */
class StartSearchIntentProcessor(
    private val metrics: MetricController
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        val event = intent.extras?.getString(HomeActivity.OPEN_TO_SEARCH)
        var source: Event.PerformedSearch.SearchAccessPoint? = null
        return if (event != null) {
            when (event) {
                SEARCH_WIDGET -> {
                    metrics.track(Event.SearchWidgetNewTabPressed)
                    source = Event.PerformedSearch.SearchAccessPoint.WIDGET
                }
                STATIC_SHORTCUT_NEW_TAB -> {
                    metrics.track(Event.PrivateBrowsingStaticShortcutTab)
                    source = Event.PerformedSearch.SearchAccessPoint.SHORTCUT
                }
                STATIC_SHORTCUT_NEW_PRIVATE_TAB -> {
                    metrics.track(Event.PrivateBrowsingStaticShortcutPrivateTab)
                    source = Event.PerformedSearch.SearchAccessPoint.SHORTCUT
                }
                PRIVATE_BROWSING_PINNED_SHORTCUT -> {
                    metrics.track(Event.PrivateBrowsingPinnedShortcutPrivateTab)
                    source = Event.PerformedSearch.SearchAccessPoint.SHORTCUT
                }
            }

            out.removeExtra(HomeActivity.OPEN_TO_SEARCH)

            val directions = source?.let {
                NavGraphDirections.actionGlobalSearchDialog(
                    sessionId = null,
                    searchAccessPoint = it
                )
            }
            directions?.let { navController.nav(null, it) }
            true
        } else {
            false
        }
    }

    companion object {
        const val SEARCH_WIDGET = "search_widget"
        const val STATIC_SHORTCUT_NEW_TAB = "static_shortcut_new_tab"
        const val STATIC_SHORTCUT_NEW_PRIVATE_TAB = "static_shortcut_new_private_tab"
        const val PRIVATE_BROWSING_PINNED_SHORTCUT = "private_browsing_pinned_shortcut"
    }
}
