/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.support.v4.media.session.MediaSessionCompat
import mozilla.components.feature.media.ext.toPlaybackState
import mozilla.components.feature.media.focus.AudioFocus
import mozilla.components.feature.media.notification.MediaNotification
import mozilla.components.feature.media.session.MediaSessionCallback
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.support.base.ids.NotificationIds
import mozilla.components.support.base.log.logger.Logger

private const val NOTIFICATION_TAG = "mozac.feature.media.foreground-service"

/**
 * A foreground service that will keep the process alive while we are playing media (with the app possibly in the
 * background) and shows an ongoing notification
 */
internal class MediaService : Service() {
    private val logger = Logger("MediaService")
    private val notification = MediaNotification(this)
    private val mediaSession by lazy { MediaSessionCompat(this, "MozacMedia") }
    private val audioFocus = AudioFocus(this)

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

        processCurrentState()

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
    }

    private fun updateNotification(state: MediaState) {
        val notificationId = NotificationIds.getIdForTag(this, NOTIFICATION_TAG)
        startForeground(notificationId, notification.create(state, mediaSession))
    }

    private fun shutdown() {
        mediaSession.release()
        stopSelf()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
