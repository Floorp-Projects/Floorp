/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.notification

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.graphics.Bitmap
import android.support.v4.media.session.MediaSessionCompat
import androidx.annotation.DrawableRes
import androidx.core.app.NotificationCompat
import mozilla.components.browser.session.Session
import mozilla.components.feature.media.R
import mozilla.components.feature.media.service.MediaService
import mozilla.components.feature.media.state.MediaState

/**
 * Helper to display a notification for web content playing media.
 */
internal class MediaNotification(
    private val context: Context
) {
    /**
     * Creates a new [Notification] for the given [state].
     */
    @Suppress("LongMethod")
    fun create(state: MediaState, mediaSession: MediaSessionCompat): Notification {
        val channel = MediaNotificationChannel.ensureChannelExists(context)

        val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)
        val pendingIntent = PendingIntent.getActivity(context, 0, intent, 0)

        val data = state.toNotificationData(context)

        val builder = NotificationCompat.Builder(context, channel)
            .setSmallIcon(data.icon)
            .setContentTitle(data.title)
            .setContentText(data.description)
            .setLargeIcon(data.largeIcon)
            .addAction(data.action)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setStyle(androidx.media.app.NotificationCompat.MediaStyle()
                .setMediaSession(mediaSession.sessionToken)
                .setShowActionsInCompactView(0))

        if (!state.isForExternalApp()) {
            // We only set a content intent if this media notification is not for an "external app"
            // like a custom tab. Currently we can't route the user to that particular activity:
            // https://github.com/mozilla-mobile/android-components/issues/3986
            builder.setContentIntent(pendingIntent)
        }

        return builder.build()
    }
}

private fun MediaState.toNotificationData(context: Context): NotificationData {
    return when (this) {
        is MediaState.Playing -> NotificationData(
            title = session.getTitleOrUrl(context),
            description = session.nonPrivateUrl,
            icon = R.drawable.mozac_feature_media_playing,
            largeIcon = session.nonPrivateIcon,
            action = NotificationCompat.Action.Builder(
                R.drawable.mozac_feature_media_action_pause,
                context.getString(R.string.mozac_feature_media_notification_action_pause),
                PendingIntent.getService(
                    context,
                    0,
                    MediaService.pauseIntent(context),
                    0)
            ).build()
        )
        is MediaState.Paused -> NotificationData(
            title = session.getTitleOrUrl(context),
            description = session.nonPrivateUrl,
            icon = R.drawable.mozac_feature_media_paused,
            largeIcon = session.nonPrivateIcon,
            action = NotificationCompat.Action.Builder(
                R.drawable.mozac_feature_media_action_play,
                context.getString(R.string.mozac_feature_media_notification_action_play),
                PendingIntent.getService(
                    context,
                    0,
                    MediaService.playIntent(context),
                    0)
            ).build()
        )
        else -> throw IllegalArgumentException("Cannot create notification for state: $this")
    }
}

private fun Session.getTitleOrUrl(context: Context): String = when {
    private -> context.getString(R.string.mozac_feature_media_notification_private_mode)
    title.isNotEmpty() -> title
    else -> url
}

private val Session.nonPrivateUrl
    get() = if (private) "" else url

private val Session.nonPrivateIcon: Bitmap?
    get() = if (private) null else icon

private data class NotificationData(
    val title: String,
    val description: String,
    @DrawableRes val icon: Int,
    val largeIcon: Bitmap? = null,
    val action: NotificationCompat.Action
)

private fun MediaState.isForExternalApp(): Boolean {
    return when (this) {
        is MediaState.Playing -> session.isCustomTabSession()
        is MediaState.Paused -> session.isCustomTabSession()
        is MediaState.None -> false
    }
}
