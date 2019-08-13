/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.app.Service
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.media.AudioManager
import android.os.IBinder
import android.support.v4.media.MediaMetadataCompat
import android.support.v4.media.session.MediaSessionCompat
import androidx.core.app.NotificationManagerCompat
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.media.ext.playIfPaused
import mozilla.components.feature.media.ext.toPlaybackState
import mozilla.components.feature.media.focus.AudioFocus
import mozilla.components.feature.media.notification.MediaNotification
import mozilla.components.feature.media.session.MediaSessionCallback
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.support.base.ids.NotificationIds
import mozilla.components.support.base.log.logger.Logger

private const val NOTIFICATION_TAG = "mozac.feature.media.foreground-service"
private const val ACTION_UODATE_STATE = "mozac.feature.media.service.UPDATE_STATE"
private const val ACTION_PLAY = "mozac.feature.media.service.PLAY"
private const val ACTION_PAUSE = "mozac.feature.media.service.PAUSE"

/**
 * A foreground service that will keep the process alive while we are playing media (with the app possibly in the
 * background) and shows an ongoing notification
 */
internal class MediaService : Service() {
    private val logger = Logger("MediaService")
    private val notification = MediaNotification(this)
    private val mediaSession by lazy { MediaSessionCompat(this, "MozacMedia") }
    private val audioFocus by lazy { AudioFocus(getSystemService(Context.AUDIO_SERVICE) as AudioManager) }

    override fun onCreate() {
        super.onCreate()

        logger.debug("Service created")

        mediaSession.setCallback(MediaSessionCallback())

        mediaSession.setFlags(
            MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS or
            MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        logger.debug("Command received")

        when (intent?.action) {
            ACTION_UODATE_STATE -> processCurrentState()
            ACTION_PLAY -> MediaStateMachine.state.playIfPaused()
            ACTION_PAUSE -> MediaStateMachine.state.pauseIfPlaying()
            else -> logger.debug("Can't process action: ${intent?.action}")
        }

        return START_NOT_STICKY
    }

    private fun processCurrentState() {
        val state = MediaStateMachine.state
        if (state == MediaState.None) {
            shutdown()
            return
        }

        if (state is MediaState.Playing) {
            audioFocus.request(state)
        }

        updateMediaSession(state)
        updateNotification(state)
    }

    private fun updateMediaSession(state: MediaState) {
        mediaSession.setPlaybackState(state.toPlaybackState())
        mediaSession.isActive = true
        mediaSession.setMetadata(MediaMetadataCompat.Builder()
            .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, -1)
            .build())
    }

    private fun updateNotification(state: MediaState) {
        val notificationId = NotificationIds.getIdForTag(this, NOTIFICATION_TAG)

        val notification = notification.create(state, mediaSession)

        if (state is MediaState.Playing) {
            startForeground(notificationId, notification)
        } else {
            stopForeground(false)

            NotificationManagerCompat.from(this)
                .notify(notificationId, notification)
        }
    }

    private fun shutdown() {
        audioFocus.abandon()
        mediaSession.release()
        stopSelf()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    companion object {
        fun updateState(context: Context) {
            context.startService(updateStateIntent(context))
        }

        fun updateStateIntent(context: Context) = Intent(ACTION_UODATE_STATE).apply {
            component = ComponentName(context, MediaService::class.java)
        }

        fun playIntent(context: Context): Intent = Intent(ACTION_PLAY).apply {
            component = ComponentName(context, MediaService::class.java)
        }

        fun pauseIntent(context: Context): Intent = Intent(ACTION_PAUSE).apply {
            component = ComponentName(context, MediaService::class.java)
        }
    }
}
