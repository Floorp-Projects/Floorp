/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioManager
import android.support.v4.media.MediaMetadataCompat
import android.support.v4.media.session.MediaSessionCompat
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationManagerCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.ext.findActiveMediaTab
import mozilla.components.feature.media.ext.getTitleOrUrl
import mozilla.components.feature.media.ext.nonPrivateUrl
import mozilla.components.feature.media.ext.toPlaybackState
import mozilla.components.feature.media.facts.emitNotificationPauseFact
import mozilla.components.feature.media.facts.emitNotificationPlayFact
import mozilla.components.feature.media.facts.emitStatePauseFact
import mozilla.components.feature.media.facts.emitStatePlayFact
import mozilla.components.feature.media.facts.emitStateStopFact
import mozilla.components.feature.media.focus.AudioFocus
import mozilla.components.feature.media.notification.MediaNotification
import mozilla.components.feature.media.session.MediaSessionCallback
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

private class BecomingNoisyReceiver(private val controller: MediaSession.Controller?) : BroadcastReceiver() {
    override fun onReceive(context: Context?, intent: Intent) {
        if (AudioManager.ACTION_AUDIO_BECOMING_NOISY == intent.action) {
            controller?.pause()
        }
    }
    @VisibleForTesting
    fun deviceIsBecomingNoisy(context: Context) {
        val becomingNoisyIntent = Intent(AudioManager.ACTION_AUDIO_BECOMING_NOISY)
        onReceive(context, becomingNoisyIntent)
    }
}

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
    private val notificationId by lazy {
        SharedIdsHelper.getIdForTag(context, AbstractMediaSessionService.NOTIFICATION_TAG)
    }
    private var scope: CoroutineScope? = null
    private var controller: MediaSession.Controller? = null
    private var notificationScope: CoroutineScope? = null

    private val intentFilter = IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY)
    private var noisyAudioStreamReceiver: BecomingNoisyReceiver? = null

    @VisibleForTesting
    internal var isForegroundService: Boolean = false

    fun onCreate() {
        logger.debug("Service created")
        mediaSession.setCallback(MediaSessionCallback(store))
        notificationScope = MainScope()

        scope = store.flowScoped { flow ->
            flow.map { state -> state.findActiveMediaTab() }
                .ifChanged { tab -> tab?.mediaSessionState }
                .collect { state -> processMediaSessionState(state) }
        }
    }

    fun onDestroy() {
        notificationScope?.cancel()
        destroy()
        logger.debug("Service destroyed")
    }

    fun onStartCommand(intent: Intent?) {
        logger.debug("Command received: ${intent?.action}")

        when (intent?.action) {
            AbstractMediaSessionService.ACTION_LAUNCH -> {
                startForegroundNotification()
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

    private fun processMediaSessionState(state: SessionState?) {
        if (state == null) {
            shutdown()
            return
        }

        when (state.mediaSessionState?.playbackState) {
            MediaSession.PlaybackState.PLAYING -> {
                registerBecomingNoisyListenerIfNeeded(state)
                audioFocus.request(state.id)
                emitStatePlayFact()
                startForegroundNotificationIfNeeded()
            }
            MediaSession.PlaybackState.PAUSED -> {
                unregisterBecomingNoisyListenerIfNeeded()
                emitStatePauseFact()
                stopForeground()
            }
            else -> {
                unregisterBecomingNoisyListenerIfNeeded()
                emitStateStopFact()
                stopForeground()
            }
        }

        updateMediaSession(state)
        notificationScope?.launch() {
            updateNotification(state)
        }
    }

    private fun updateMediaSession(sessionState: SessionState) {
        mediaSession.setPlaybackState(sessionState.mediaSessionState?.toPlaybackState())
        mediaSession.isActive = true
        mediaSession.setMetadata(
            MediaMetadataCompat.Builder()
                .putString(
                    MediaMetadataCompat.METADATA_KEY_TITLE,
                    sessionState.getTitleOrUrl(context)
                )
                .putString(
                    MediaMetadataCompat.METADATA_KEY_ARTIST,
                    sessionState.nonPrivateUrl
                )
                .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, -1)
                .build()
        )
    }

    private fun startForegroundNotification() {
        val notification = notification.createDummy(mediaSession)
        service.startForeground(notificationId, notification)
        isForegroundService = true
    }

    private fun startForegroundNotificationIfNeeded() {
        if (!isForegroundService) {
            startForegroundNotification()
        }
    }

    private fun stopForeground() {
        service.stopForeground(false)
        isForegroundService = false
    }

    private fun registerBecomingNoisyListenerIfNeeded(state: SessionState) {
        if (noisyAudioStreamReceiver != null) {
            return
        }

        noisyAudioStreamReceiver = BecomingNoisyReceiver(state.mediaSessionState?.controller)
        context.registerReceiver(noisyAudioStreamReceiver, intentFilter)
    }

    private fun unregisterBecomingNoisyListenerIfNeeded() {
        noisyAudioStreamReceiver?.let {
            context.unregisterReceiver(noisyAudioStreamReceiver)
            noisyAudioStreamReceiver = null
        }
    }

    private suspend fun updateNotification(sessionState: SessionState?) {
        val notification = notification.create(sessionState, mediaSession)

        sessionState?.mediaSessionState?.let {
            controller = it.controller

            NotificationManagerCompat.from(context).notify(notificationId, notification)
        }
    }

    @VisibleForTesting
    internal fun destroy() {
        scope?.cancel()
        audioFocus.abandon()
    }

    @VisibleForTesting
    internal fun shutdown() {
        mediaSession.release()
        service.stopSelf()
    }

    @VisibleForTesting
    internal fun deviceBecomingNoisy(context: Context) {
        noisyAudioStreamReceiver?.deviceIsBecomingNoisy(context)
    }
}
