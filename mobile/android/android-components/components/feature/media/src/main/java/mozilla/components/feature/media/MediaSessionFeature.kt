/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import android.content.Context
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.service.AbstractMediaSessionService
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature implementation that handles MediaSession state changes
 *
 * @param applicationContext the application's [Context].
 * @param mediaServiceClass the media service class will handle the media playback state
 * @param store Reference to the browser store where tab state is located.
 */
class MediaSessionFeature(
    val applicationContext: Context,
    val mediaServiceClass: Class<*>,
    val store: BrowserStore
) {
    private var scope: CoroutineScope? = null

    /**
     * Starts the feature.
     */
    fun start() {
        scope = store.flowScoped { flow ->
            flow.map {
                it.tabs + it.customTabs
            }.map { tab ->
                tab.none {
                    it.mediaSessionState != null &&
                        it.mediaSessionState!!.playbackState == MediaSession.PlaybackState.PLAYING
                }
            }.ifChanged().collect { isEmpty ->
                process(isEmpty)
            }
        }
    }

    /**
     * Stops the feature.
     */
    fun stop() {
        scope?.cancel()
    }

    private fun process(isEmpty: Boolean) {
        if (!isEmpty) {
            launch()
        }
    }

    private fun launch() {
        ContextCompat.startForegroundService(
            applicationContext,
            AbstractMediaSessionService.launchIntent(
                applicationContext,
                mediaServiceClass
            )
        )
    }
}
