/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.notification

import android.app.Notification
import android.app.PendingIntent
import android.app.PendingIntent.FLAG_UPDATE_CURRENT
import android.content.Context
import android.graphics.Bitmap
import android.os.Build
import android.support.v4.media.session.MediaSessionCompat
import androidx.annotation.DrawableRes
import androidx.core.app.NotificationCompat
import androidx.media.app.NotificationCompat.MediaStyle
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.R
import mozilla.components.feature.media.ext.getNonPrivateIcon
import mozilla.components.feature.media.ext.getTitleOrUrl
import mozilla.components.feature.media.ext.nonPrivateUrl
import mozilla.components.feature.media.service.AbstractMediaSessionService
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.utils.PendingIntentUtils
import java.util.Locale

/**
 * Helper to display a notification for web content playing media.
 */
internal class MediaNotification(
    private val context: Context,
    private val cls: Class<*>,
) {
    /**
     * Creates a new [Notification] for the given [sessionState].
     */
    suspend fun create(sessionState: SessionState?, mediaSessionCompat: MediaSessionCompat): Notification {
        val data = sessionState?.toNotificationData(context, cls) ?: NotificationData()

        return buildNotification(data, mediaSessionCompat, sessionState !is CustomTabSessionState)
    }

    private fun buildNotification(
        data: NotificationData,
        mediaSession: MediaSessionCompat,
        isCustomTab: Boolean,
    ): Notification {
        val channel = MediaNotificationChannel.ensureChannelExists(context)
        val style = MediaStyle().setMediaSession(mediaSession.sessionToken)
        val builder = NotificationCompat.Builder(context, channel)
            .setSmallIcon(data.icon)
            .setContentTitle(data.title)
            .setContentText(data.description)
            .setLargeIcon(data.largeIcon)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)

        if (data.action != null) {
            builder.addAction(data.action)
            style.setShowActionsInCompactView(0)
        }

        // There is a known OEM crash with Huawei Devices on lollipop with setting a style
        // see https://github.com/mozilla-mobile/android-components/issues/7468 and
        // https://issuetracker.google.com/issues/37078372
        val huaweiOnLollipop =
            Build.MANUFACTURER.lowercase(Locale.getDefault()).contains("huawei") &&
                Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1
        if (!huaweiOnLollipop) {
            builder.setStyle(style)
        }

        if (isCustomTab) {
            // We only set a content intent if this media notification is not for an "external app"
            // like a custom tab. Currently we can't route the user to that particular activity:
            // https://github.com/mozilla-mobile/android-components/issues/3986
            builder.setContentIntent(data.contentIntent)
        }

        return builder.build()
    }
}

private suspend fun SessionState.toNotificationData(
    context: Context,
    cls: Class<*>,
): NotificationData {
    val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)?.also {
        it.action = AbstractMediaSessionService.ACTION_SWITCH_TAB
    }

    return when (mediaSessionState?.playbackState) {
        MediaSession.PlaybackState.PLAYING -> NotificationData(
            title = getTitleOrUrl(context, mediaSessionState?.metadata?.title),
            description = nonPrivateUrl,
            icon = R.drawable.mozac_feature_media_playing,
            largeIcon = getNonPrivateIcon(mediaSessionState?.metadata?.getArtwork),
            action = NotificationCompat.Action.Builder(
                R.drawable.mozac_feature_media_action_pause,
                context.getString(R.string.mozac_feature_media_notification_action_pause),
                PendingIntent.getService(
                    context,
                    0,
                    AbstractMediaSessionService.pauseIntent(context, cls),
                    getNotificationFlag(),
                ),
            ).build(),
            contentIntent = PendingIntent.getActivity(
                context,
                SharedIdsHelper.getIdForTag(context, AbstractMediaSessionService.PENDING_INTENT_TAG),
                intent?.apply { putExtra(AbstractMediaSessionService.EXTRA_TAB_ID, id) },
                getUpdateNotificationFlag(),
            ),
        )
        MediaSession.PlaybackState.PAUSED -> NotificationData(
            title = getTitleOrUrl(context, mediaSessionState?.metadata?.title),
            description = nonPrivateUrl,
            icon = R.drawable.mozac_feature_media_paused,
            largeIcon = getNonPrivateIcon(mediaSessionState?.metadata?.getArtwork),
            action = NotificationCompat.Action.Builder(
                R.drawable.mozac_feature_media_action_play,
                context.getString(R.string.mozac_feature_media_notification_action_play),
                PendingIntent.getService(
                    context,
                    0,
                    AbstractMediaSessionService.playIntent(context, cls),
                    getNotificationFlag(),
                ),
            ).build(),
            contentIntent = PendingIntent.getActivity(
                context,
                SharedIdsHelper.getIdForTag(context, AbstractMediaSessionService.PENDING_INTENT_TAG),
                intent?.apply { putExtra(AbstractMediaSessionService.EXTRA_TAB_ID, id) },
                getUpdateNotificationFlag(),
            ),
        )
        // Dummy notification used of all other media states.
        else -> NotificationData()
    }
}

private data class NotificationData(
    val title: String = "",
    val description: String = "",
    @DrawableRes val icon: Int = R.drawable.mozac_feature_media_playing,
    val largeIcon: Bitmap? = null,
    val action: NotificationCompat.Action? = null,
    val contentIntent: PendingIntent? = null,
)

private fun getNotificationFlag() = PendingIntentUtils.defaultFlags

private fun getUpdateNotificationFlag() = PendingIntentUtils.defaultFlags or FLAG_UPDATE_CURRENT
