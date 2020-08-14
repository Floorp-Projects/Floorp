/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.content.Intent
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.privatemode.notification.AbstractPrivateNotificationService
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics

/**
 * Manages notifications for private tabs.
 *
 * Private tab notifications solve two problems for us:
 * 1 - They allow users to interact with us from outside of the app (example: by closing all
 * private tabs).
 * 2 - The notification will keep our process alive, allowing us to keep private tabs in memory.
 *
 * As long as a session is active this service will keep its notification alive.
 */
class PrivateNotificationService : AbstractPrivateNotificationService() {

    override val store: BrowserStore by lazy { components.core.store }

    override fun NotificationCompat.Builder.buildNotification() {
        setSmallIcon(R.drawable.ic_pbm_notification)
        setContentTitle(getString(R.string.app_name_private_4, getString(R.string.app_name)))
        setContentText(getString(R.string.notification_pbm_delete_text_2))
        color = ContextCompat.getColor(this@PrivateNotificationService, R.color.pbm_notification_color)
    }

    override fun erasePrivateTabs() {
        metrics.track(Event.PrivateBrowsingNotificationTapped)

        val homeScreenIntent = Intent(this, HomeActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            putExtra(HomeActivity.PRIVATE_BROWSING_MODE, isStartedFromPrivateShortcut)
        }

        if (VisibilityLifecycleCallback.finishAndRemoveTaskIfInBackground(this)) {
            // Set start mode to be in background (recents screen)
            homeScreenIntent.apply {
                putExtra(HomeActivity.START_IN_RECENTS_SCREEN, true)
            }
        }

        startActivity(homeScreenIntent)
        super.erasePrivateTabs()
    }

    companion object {

        /**
         * Global used by [HomeActivity] to figure out if normal mode or private mode
         * should be used after closing all private tabs.
         */
        var isStartedFromPrivateShortcut = false
    }
}
