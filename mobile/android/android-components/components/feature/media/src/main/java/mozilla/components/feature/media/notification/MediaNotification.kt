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
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.media.R
import mozilla.components.feature.media.ext.getActiveMediaTab
import mozilla.components.feature.media.ext.getTitleOrUrl
import mozilla.components.feature.media.ext.isMediaStateForCustomTab
import mozilla.components.feature.media.ext.nonPrivateIcon
import mozilla.components.feature.media.ext.nonPrivateUrl
import mozilla.components.feature.media.service.AbstractMediaService
import mozilla.components.support.base.ids.SharedIdsHelper

/**
 * Helper to display a notification for web content playing media.
 */
internal class MediaNotification(
    private val context: Context,
    private val cls: Class<*>
) {
    /**
     * Creates a new [Notification] for the given [state].
     */
    @Suppress("LongMethod")
    fun create(state: BrowserState, mediaSession: MediaSessionCompat): Notification {
        val channel = MediaNotificationChannel.ensureChannelExists(context)

        val data = state.toNotificationData(context, cls)

        val builder = NotificationCompat.Builder(context, channel)
            .setSmallIcon(data.icon)
            .setContentTitle(data.title)
            .setContentText(data.description)
            .setLargeIcon(data.largeIcon)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)

        val style = androidx.media.app.NotificationCompat.MediaStyle()
            .setMediaSession(mediaSession.sessionToken)

        if (data.action != null) {
            builder.addAction(data.action)

            style.setShowActionsInCompactView(0)
        }

        builder.setStyle(style)

        if (!state.isMediaStateForCustomTab()) {
            // We only set a content intent if this media notification is not for an "external app"
            // like a custom tab. Currently we can't route the user to that particular activity:
            // https://github.com/mozilla-mobile/android-components/issues/3986
            builder.setContentIntent(data.contentIntent)
        }

        return builder.build()
    }
}

private fun BrowserState.toNotificationData(
    context: Context,
    cls: Class<*>
): NotificationData {
    val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)?.also {
        it.action = AbstractMediaService.ACTION_SWITCH_TAB
    }

    val mediaTab = getActiveMediaTab()

    return when (media.aggregate.state) {
        MediaState.State.PLAYING -> NotificationData(
            title = mediaTab.getTitleOrUrl(context),
            description = mediaTab.nonPrivateUrl,
            icon = R.drawable.mozac_feature_media_playing,
            largeIcon = mediaTab.nonPrivateIcon,
            action = NotificationCompat.Action.Builder(
                R.drawable.mozac_feature_media_action_pause,
                context.getString(R.string.mozac_feature_media_notification_action_pause),
                PendingIntent.getService(
                    context,
                    0,
                    AbstractMediaService.pauseIntent(context, cls),
                    0)
            ).build(),
            contentIntent = PendingIntent.getActivity(
                context,
                SharedIdsHelper.getIdForTag(context, AbstractMediaService.PENDING_INTENT_TAG),
                intent?.apply { putExtra(AbstractMediaService.EXTRA_TAB_ID, mediaTab?.id) },
                PendingIntent.FLAG_UPDATE_CURRENT
            )
        )
        MediaState.State.PAUSED -> NotificationData(
            title = mediaTab.getTitleOrUrl(context),
            description = mediaTab.nonPrivateUrl,
            icon = R.drawable.mozac_feature_media_paused,
            largeIcon = mediaTab.nonPrivateIcon,
            action = NotificationCompat.Action.Builder(
                R.drawable.mozac_feature_media_action_play,
                context.getString(R.string.mozac_feature_media_notification_action_play),
                PendingIntent.getService(
                    context,
                    0,
                    AbstractMediaService.playIntent(context, cls),
                    0)
            ).build(),
            contentIntent = PendingIntent.getActivity(
                context,
                SharedIdsHelper.getIdForTag(context, AbstractMediaService.PENDING_INTENT_TAG),
                intent?.apply { putExtra(AbstractMediaService.EXTRA_TAB_ID, mediaTab?.id) },
                PendingIntent.FLAG_UPDATE_CURRENT
            )
        )
        // Dummy notification that is only used to satisfy the requirement to ALWAYS call
        // startForeground with a notification.
        else -> NotificationData()
    }
}

private data class NotificationData(
    val title: String = "",
    val description: String = "",
    @DrawableRes val icon: Int = R.drawable.mozac_feature_media_playing,
    val largeIcon: Bitmap? = null,
    val action: NotificationCompat.Action? = null,
    val contentIntent: PendingIntent? = null
)
