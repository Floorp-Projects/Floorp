/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser

import android.content.Context
import androidx.preference.PreferenceManager
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.focus.R
import org.mozilla.focus.ext.settings

/**
 * [Middleware] to record the number of blocked trackers in response to [BrowserAction]s.
 * @param context The application context.
 */
class BlockedTrackersMiddleware(
    private val context: Context,
) : Middleware<BrowserState, BrowserAction> {

    private val settings = context.settings
    private val preferences = PreferenceManager.getDefaultSharedPreferences(context)

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is TrackingProtectionAction.TrackerBlockedAction -> {
                incrementCount()
            }
            else -> {
                // no-op
            }
        }

        next(action)
    }

    private fun incrementCount() {
        val blockedTrackersCount = settings.getTotalBlockedTrackersCount()
        preferences
            .edit()
            .putInt(
                context.getString(R.string.pref_key_privacy_total_trackers_blocked_count),
                blockedTrackersCount + 1,
            )
            .apply()
    }
}
