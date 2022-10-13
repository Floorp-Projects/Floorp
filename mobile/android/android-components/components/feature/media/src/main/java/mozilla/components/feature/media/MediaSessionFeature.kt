/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession.PlaybackState.PAUSED
import mozilla.components.concept.engine.mediasession.MediaSession.PlaybackState.PLAYING
import mozilla.components.feature.media.ext.findActiveMediaTab
import mozilla.components.feature.media.service.MediaServiceBinder
import mozilla.components.feature.media.service.MediaSessionDelegate
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature implementation that handles MediaSession state changes and controls showing a notification
 * reflecting the media states.
 *
 * @param applicationContext the application's [Context].
 * @param mediaServiceClass the media service class will handle the media playback state
 * @param store Reference to the browser store where tab state is located.
 */
class MediaSessionFeature(
    val applicationContext: Context,
    val mediaServiceClass: Class<*>,
    val store: BrowserStore,
) {
    @VisibleForTesting
    internal var scope: CoroutineScope? = null

    @VisibleForTesting
    internal var mediaService: MediaSessionDelegate? = null

    @VisibleForTesting
    internal val mediaServiceConnection = object : ServiceConnection {
        override fun onServiceConnected(className: ComponentName, binder: IBinder) {
            val serviceBinder = binder as MediaServiceBinder
            mediaService = serviceBinder.getMediaService()
            // The service is bound when media starts playing. Ensure in-progress media status is correctly shown.
            store.state.findActiveMediaTab()?.let {
                showMediaStatus(it)
            }
        }

        override fun onServiceDisconnected(className: ComponentName?) {
            mediaService = null
        }
    }

    /**
     * Starts the feature.
     */
    fun start() {
        scope = store.flowScoped { flow ->
            flow.map { state -> state.findActiveMediaTab() }
                .ifChanged { tab -> tab?.mediaSessionState }
                .collect { state -> showMediaStatus(state) }
        }
    }

    /**
     * Stops the feature.
     */
    fun stop() {
        scope?.cancel()
        scope = null
    }

    @VisibleForTesting
    internal fun showMediaStatus(sessionState: SessionState?) {
        if (sessionState == null) {
            mediaService?.let {
                it.handleNoMedia()
                applicationContext.unbindService(mediaServiceConnection)
                mediaService = null
            }

            return
        }

        when (sessionState.mediaSessionState?.playbackState) {
            PLAYING -> {
                if (mediaService == null) {
                    applicationContext.bindService(
                        Intent(applicationContext, mediaServiceClass),
                        mediaServiceConnection,
                        Context.BIND_AUTO_CREATE,
                    )
                }

                mediaService?.handleMediaPlaying(sessionState)
            }
            PAUSED -> mediaService?.handleMediaPaused(sessionState)
            else -> mediaService?.handleMediaStopped(sessionState)
        }
    }
}
