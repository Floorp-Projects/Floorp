/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.notification

import android.content.Context
import android.content.Intent
import mozilla.components.feature.media.service.MediaService
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine
import java.lang.ref.WeakReference

/**
 * Feature for displaying an ongoing notification (keeping the app process alive) while web content is playing media.
 *
 * This feature should get initialized globally once on app start.
 */
class MediaNotificationFeature(
    private val context: Context,
    private val stateMachine: MediaStateMachine
) {
    private var serviceRunning = false

    /**
     * Enables the feature.
     */
    fun enable() {
        stateMachine.register(MediaObserver(this))
    }

    internal fun startMediaService(state: MediaState) {
        lastState = WeakReference(state)

        context.startService(Intent(context, MediaService::class.java))

        serviceRunning = true
    }

    internal fun stopMediaService() {
        if (serviceRunning) {
            lastState.clear()

            context.stopService(Intent(context, MediaService::class.java))
        }
    }

    companion object {
        private var lastState = WeakReference<MediaState>(null)

        internal fun getState(): MediaState {
            return lastState.get() ?: MediaState.None
        }
    }
}

internal class MediaObserver(
    private val feature: MediaNotificationFeature
) : MediaStateMachine.Observer {
    override fun onStateChanged(state: MediaState) {
        if (state is MediaState.Playing || state is MediaState.Paused) {
            feature.startMediaService(state)
        } else if (state is MediaState.None) {
            feature.stopMediaService()
        }
    }
}
