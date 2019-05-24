/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.app.Service
import android.content.Intent
import android.os.IBinder
import mozilla.components.feature.media.notification.MediaNotification
import mozilla.components.feature.media.notification.MediaNotificationFeature
import mozilla.components.feature.media.state.MediaState
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

    override fun onCreate() {
        super.onCreate()

        logger.debug("Service created")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        logger.debug("Command received")

        updateNotification()

        return START_NOT_STICKY
    }

    private fun updateNotification() {
        val state = MediaNotificationFeature.getState()
        if (state == MediaState.None) {
            stopSelf()
            return
        }

        val notificationId = NotificationIds.getIdForTag(this, NOTIFICATION_TAG)
        startForeground(notificationId, notification.create(state))
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
