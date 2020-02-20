/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.widget.Toast
import androidx.core.app.NotificationCompat
import androidx.core.content.getSystemService
import mozilla.components.browser.session.Session
import mozilla.components.feature.pwa.R
import mozilla.components.feature.session.SessionUseCases

/**
 * Callback for [WebAppSiteControlsFeature] that lets the displayed notification be customized.
 */
interface SiteControlsBuilder {

    /**
     * Create the notification to be displayed. Initial values are set in the provided [builder]
     * and additional actions can be added here. Actions should be represented as [PendingIntent]
     * that are filtered by [getFilter] and handled in [onReceiveBroadcast].
     */
    fun buildNotification(context: Context, builder: NotificationCompat.Builder, channelId: String)

    /**
     * Return an intent filter that matches the actions specified in [buildNotification].
     */
    fun getFilter(): IntentFilter

    /**
     * Handle actions the user selected in the site controls notification.
     */
    fun onReceiveBroadcast(context: Context, session: Session, intent: Intent)

    /**
     * Default implementation of [SiteControlsBuilder] that copies the URL of the site when tapped.
     */
    open class Default : SiteControlsBuilder {

        override fun getFilter() = IntentFilter().apply {
            addAction(ACTION_COPY)
        }

        override fun buildNotification(context: Context, builder: NotificationCompat.Builder, channelId: String) {
            val copyIntent = createPendingIntent(context, ACTION_COPY, 1)

            builder.setContentText(context.getString(R.string.mozac_feature_pwa_site_controls_notification_text))
            builder.setContentIntent(copyIntent)
        }

        override fun onReceiveBroadcast(context: Context, session: Session, intent: Intent) {
            when (intent.action) {
                ACTION_COPY -> {
                    context.getSystemService<ClipboardManager>()?.let { clipboardManager ->
                        clipboardManager.setPrimaryClip(ClipData.newPlainText(session.url, session.url))
                        Toast.makeText(
                            context,
                            context.getString(R.string.mozac_feature_pwa_copy_success),
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
            }
        }

        protected fun createPendingIntent(context: Context, action: String, requestCode: Int): PendingIntent {
            val intent = Intent(action)
            intent.setPackage(context.packageName)
            return PendingIntent.getBroadcast(context, requestCode, intent, 0)
        }

        companion object {
            private const val ACTION_COPY = "mozilla.components.feature.pwa.COPY"
        }
    }

    /**
     * Implementation of [SiteControlsBuilder] that adds a Refresh button and
     * copies the URL of the site when tapped.
     */
    class CopyAndRefresh(
        private val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase
    ) : Default() {

        override fun getFilter() = super.getFilter().apply {
            addAction(ACTION_REFRESH)
        }

        override fun buildNotification(context: Context, builder: NotificationCompat.Builder, channelId: String) {
            super.buildNotification(context, builder, channelId)
            val refreshAction = NotificationCompat.Action(
                R.drawable.ic_refresh,
                context.getString(R.string.mozac_feature_pwa_site_controls_refresh),
                createPendingIntent(context, ACTION_REFRESH, 2)
            )

            builder.addAction(refreshAction)
        }

        override fun onReceiveBroadcast(context: Context, session: Session, intent: Intent) {
            when (intent.action) {
                ACTION_REFRESH -> reloadUrlUseCase(session)
                else -> super.onReceiveBroadcast(context, session, intent)
            }
        }

        companion object {
            private const val ACTION_REFRESH = "mozilla.components.feature.pwa.REFRESH"
        }
    }
}
