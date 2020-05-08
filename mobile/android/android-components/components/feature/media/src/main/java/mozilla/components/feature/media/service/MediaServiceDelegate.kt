/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.content.Context
import android.content.Intent
import android.media.AudioManager
import android.support.v4.media.MediaMetadataCompat
import android.support.v4.media.session.MediaSessionCompat
import androidx.core.app.NotificationManagerCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.media.ext.getActiveMediaTab
import mozilla.components.feature.media.ext.getTitleOrUrl
import mozilla.components.feature.media.ext.isMediaStateForCustomTab
import mozilla.components.feature.media.ext.nonPrivateUrl
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.media.ext.playIfPaused
import mozilla.components.feature.media.ext.toPlaybackState
import mozilla.components.feature.media.facts.emitNotificationPauseFact
import mozilla.components.feature.media.facts.emitNotificationPlayFact
import mozilla.components.feature.media.focus.AudioFocus
import mozilla.components.feature.media.notification.MediaNotification
import mozilla.components.feature.media.session.MediaSessionCallback
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Delegate handling callbacks from an [AbstractMediaService].
 *
 * The implementation was moved from [AbstractMediaService] to this delegate for better testability.
 */
internal class MediaServiceDelegate(
    private val context: Context,
    private val service: AbstractMediaService,
    private val store: BrowserStore
) {
    private val logger = Logger("MediaService")
    private val notification by lazy { MediaNotification(context, service::class.java) }
    private val mediaSession: MediaSessionCompat by lazy { MediaSessionCompat(context, "MozacMedia") }
    private val audioFocus: AudioFocus by lazy {
        AudioFocus(context.getSystemService(Context.AUDIO_SERVICE) as AudioManager, store)
    }
    private var scope: CoroutineScope? = null

    fun onCreate() {
        logger.debug("Service created")

        mediaSession.setCallback(MediaSessionCallback(store))

        scope = store.flowScoped { flow ->
            flow
                .ifChanged { state -> state.media.aggregate }
                .collect { state -> processState(state) }
        }
    }

    fun onDestroy() {
        scope?.cancel()

        logger.debug("Service destroyed")
    }

    fun onStartCommand(intent: Intent?) {
        logger.debug("Command received: ${intent?.action}")

        when (intent?.action) {
            AbstractMediaService.ACTION_LAUNCH -> {
                // Nothing to do here. The service will subscribe to the store in onCreate() and
                // update its state from the store.
            }
            AbstractMediaService.ACTION_PLAY -> {
                store.state.media.playIfPaused()
                emitNotificationPlayFact()
            }
            AbstractMediaService.ACTION_PAUSE -> {
                store.state.media.pauseIfPlaying()
                emitNotificationPauseFact()
            }
            else -> logger.debug("Can't process action: ${intent?.action}")
        }
    }

    fun onTaskRemoved() {
        val state = store.state

        if (state.isMediaStateForCustomTab()) {
            // Custom Tabs have their own lifetime management (bound to the lifetime of the activity)
            // and do not need to be handled here.
            return
        }

        state.media.pauseIfPlaying()

        shutdown()
    }

    private fun processState(state: BrowserState) {
        if (state.media.aggregate.state == MediaState.State.NONE) {
            updateNotification(state)
            shutdown()
            return
        }

        if (state.media.aggregate.state == MediaState.State.PLAYING) {
            audioFocus.request(state.media)
        }

        updateMediaSession(state)
        updateNotification(state)
    }

    private fun updateMediaSession(state: BrowserState) {
        mediaSession.setPlaybackState(state.media.toPlaybackState())
        mediaSession.isActive = true
        mediaSession.setMetadata(
            MediaMetadataCompat.Builder()
                .putString(
                    MediaMetadataCompat.METADATA_KEY_TITLE,
                    state.getActiveMediaTab().getTitleOrUrl(context))
                .putString(
                    MediaMetadataCompat.METADATA_KEY_ARTIST,
                    state.getActiveMediaTab().nonPrivateUrl)
                .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, -1)
                .build())
    }

    private fun updateNotification(state: BrowserState) {
        val notificationId = SharedIdsHelper.getIdForTag(
            context,
            AbstractMediaService.NOTIFICATION_TAG
        )

        val notification = notification.create(state, mediaSession)

        // Android wants us to always, always, ALWAYS call startForeground() if
        // startForegroundService() was invoked. Even if we already did that for this service or
        // if we are stopping foreground or stopping the whole service. No matter what. Always
        // call startForeground().
        service.startForeground(notificationId, notification)

        if (state.media.aggregate.state != MediaState.State.PLAYING) {
            service.stopForeground(false)

            NotificationManagerCompat.from(context)
                .notify(notificationId, notification)
        }

        if (state.media.aggregate.state == MediaState.State.NONE) {
            service.stopForeground(true)
        }
    }

    private fun shutdown() {
        audioFocus.abandon()
        mediaSession.release()
        service.stopSelf()
    }
}
