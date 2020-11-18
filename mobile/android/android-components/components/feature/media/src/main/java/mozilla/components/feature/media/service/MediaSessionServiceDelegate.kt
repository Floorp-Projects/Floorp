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
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.ext.getTitleOrUrl
import mozilla.components.feature.media.ext.nonPrivateUrl
import mozilla.components.feature.media.ext.toPlaybackState
import mozilla.components.feature.media.facts.emitNotificationPauseFact
import mozilla.components.feature.media.facts.emitNotificationPlayFact
import mozilla.components.feature.media.facts.emitStatePauseFact
import mozilla.components.feature.media.facts.emitStatePlayFact
import mozilla.components.feature.media.facts.emitStateStopFact
import mozilla.components.feature.media.notification.MediaNotification
import mozilla.components.feature.media.focus.AudioFocus
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Delegate handling callbacks from an [AbstractMediaSessionService].
 *
 * The implementation was moved from [AbstractMediaSessionService] to this delegate for better testability.
 */
internal class MediaSessionServiceDelegate(
    private val context: Context,
    private val service: AbstractMediaSessionService,
    private val store: BrowserStore
) {
    private val logger = Logger("MediaSessionService")
    private val notification by lazy { MediaNotification(context, service::class.java) }
    private val mediaSession: MediaSessionCompat by lazy { MediaSessionCompat(context, "MozacMediaSession") }
    private val audioFocus: AudioFocus by lazy {
        AudioFocus(context.getSystemService(Context.AUDIO_SERVICE) as AudioManager, store)
    }
    private var scope: CoroutineScope? = null
    private var controller: MediaSession.Controller? = null

    fun onCreate() {
        logger.debug("Service created")
        scope = store.flowScoped { flow ->
            flow.map {
                it.tabs + it.customTabs
            }.map { tab ->
                tab.filter { it.mediaSessionState != null &&
                    it.mediaSessionState!!.playbackState != MediaSession.PlaybackState.UNKNOWN }
            }.ifChanged().collect { states ->
                processMediaSessionStates(states)
            }
        }
    }

    fun onDestroy() {
        scope?.cancel()

        logger.debug("Service destroyed")
    }

    fun onStartCommand(intent: Intent?) {
        logger.debug("Command received: ${intent?.action}")

        when (intent?.action) {
            AbstractMediaSessionService.ACTION_LAUNCH -> {
                // Nothing to do here. The service will subscribe to the store in onCreate() and
                // update its state from the store.
            }
            AbstractMediaSessionService.ACTION_PLAY -> {
                controller?.play()
                emitNotificationPlayFact()
            }
            AbstractMediaSessionService.ACTION_PAUSE -> {
                controller?.pause()
                emitNotificationPauseFact()
            }
            else -> logger.debug("Can't process action: ${intent?.action}")
        }
    }

    fun onTaskRemoved() {
        /* no need to do this for custom tabs */
        store.state.tabs.forEach {
            it.mediaSessionState?.controller?.stop()
        }

        shutdown()
    }

    private fun processMediaSessionStates(sessionStates: List<SessionState>) {
        val activeState = sessionStates.sortedByDescending { it.mediaSessionState }.firstOrNull()

        if (activeState == null) {
            updateNotification(activeState)
            shutdown()
            return
        }

        when (activeState.mediaSessionState?.playbackState) {
            MediaSession.PlaybackState.PLAYING -> {
                audioFocus.request(activeState.id)
                emitStatePlayFact()
            }
            MediaSession.PlaybackState.PAUSED -> emitStatePauseFact()
            else -> emitStateStopFact()
        }

        updateMediaSession(activeState)
        updateNotification(activeState)
    }

    private fun updateMediaSession(sessionState: SessionState) {
        mediaSession.setPlaybackState(sessionState.mediaSessionState?.toPlaybackState())
        mediaSession.isActive = true
        mediaSession.setMetadata(
            MediaMetadataCompat.Builder()
                .putString(
                    MediaMetadataCompat.METADATA_KEY_TITLE,
                    sessionState.getTitleOrUrl(context))
                .putString(
                    MediaMetadataCompat.METADATA_KEY_ARTIST,
                    sessionState.nonPrivateUrl)
                .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, -1)
                .build())
    }

    private fun updateNotification(sessionState: SessionState?) {
        controller = sessionState?.mediaSessionState?.controller
        val notificationId = SharedIdsHelper.getIdForTag(
            context,
            AbstractMediaSessionService.NOTIFICATION_TAG
        )

        val notification = notification.create(sessionState, mediaSession)

        // Android wants us to always, always, ALWAYS call startForeground() if
        // startForegroundService() was invoked. Even if we already did that for this service or
        // if we are stopping foreground or stopping the whole service. No matter what. Always
        // call startForeground().
        service.startForeground(notificationId, notification)

        val mediaSessionState = sessionState?.mediaSessionState
        if (mediaSessionState != null &&
            mediaSessionState.playbackState != MediaSession.PlaybackState.PLAYING) {
            service.stopForeground(false)

            NotificationManagerCompat.from(context)
                .notify(notificationId, notification)
        }

        if (mediaSessionState == null) {
            service.stopForeground(true)
        }
    }

    private fun shutdown() {
        mediaSession.release()
        service.stopSelf()
    }
}
