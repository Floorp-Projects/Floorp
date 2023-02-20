/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.annotation.SuppressLint
import android.content.Intent
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.privatemode.notification.AbstractPrivateNotificationService
import mozilla.components.support.base.android.NotificationsDelegate
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import java.util.Locale

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

    override val notificationsDelegate: NotificationsDelegate by lazy { components.notificationsDelegate }

    override fun NotificationCompat.Builder.buildNotification() {
        setSmallIcon(R.drawable.ic_private_browsing)
        setContentTitle(
            applicationContext.getString(
                R.string.app_name_private_4,
                getString(R.string.app_name),
            ),
        )
        setContentText(
            applicationContext.getString(
                R.string.notification_pbm_delete_text_2,
            ),
        )
        color = ContextCompat.getColor(
            this@PrivateNotificationService,
            R.color.pbm_notification_color,
        )
    }

    /**
     * Update the existing notification when the [Locale] has been changed.
     */
    override fun notifyLocaleChanged() {
        super.refreshNotification()
    }

    @SuppressLint("MissingSuperCall")
    override fun erasePrivateTabs() {
        val inPrivateMode = store.state.selectedTab?.content?.private ?: false

        // Trigger use case directly for now (instead of calling super.erasePrivateTabs)
        // as otherwise SessionManager and the store will be out of sync.
        components.useCases.tabsUseCases.removePrivateTabs()

        // If the app is in private mode we launch to the private mode home screen as a
        // confirmation that all private tabs have been deleted. If we don't do this the user
        // will end up on a new selected tab in normal mode which isn't desired.
        // If the app is in normal mode there's no reason to direct the user away to
        // private mode as all private tabs have been deleted.
        if (inPrivateMode) {
            val homeScreenIntent = Intent(this, HomeActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                putExtra(HomeActivity.PRIVATE_BROWSING_MODE, true)
            }

            if (VisibilityLifecycleCallback.finishAndRemoveTaskIfInBackground(this)) {
                // Set start mode to be in background (recents screen)
                homeScreenIntent.apply {
                    putExtra(HomeActivity.START_IN_RECENTS_SCREEN, true)
                }
            }

            startActivity(homeScreenIntent)
        }
    }
}
