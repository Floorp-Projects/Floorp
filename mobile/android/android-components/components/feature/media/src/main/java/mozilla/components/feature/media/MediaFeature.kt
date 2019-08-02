/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import android.content.Context
import mozilla.components.feature.media.ext.hasMediaWithSufficientLongDuration
import mozilla.components.feature.media.service.MediaService
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine

/**
 * Feature implementation for media playback in web content. This feature takes care of:
 * - Background playback without the app getting killed.
 * - Showing a media notification with play/pause controls.
 * - Audio Focus handling (pausing/resuming in agreement with other media apps).
 * - Support for hardware controls to toggle play/pause (e.g. buttons on a headset)
 *
 * This feature should get initialized globally once on app start and requires a started
 * [MediaStateMachine].
 */
class MediaFeature(
    private val context: Context
) {
    private var serviceStarted = false
    private var observer = object : MediaStateMachine.Observer {
        override fun onStateChanged(state: MediaState) {
            notifyService(state)
        }
    }

    /**
     * Enables the feature.
     */
    fun enable() {
        MediaStateMachine.register(observer)

        // MediaStateMachine will only notify us about state changes but not pass the current state
        // to us - which might already be "playing". So let's process the current state now.
        notifyService(MediaStateMachine.state)
    }

    @Suppress("LongMethod")
    private fun notifyService(state: MediaState) {
        when (state) {
            is MediaState.Playing -> {
                if (state.hasMediaWithSufficientLongDuration()) {
                    MediaService.updateState(context)
                    serviceStarted = true
                }
            }

            is MediaState.Paused -> {
                if (serviceStarted) {
                    MediaService.updateState(context)
                }
            }

            is MediaState.None -> {
                if (serviceStarted) {
                    MediaService.updateState(context)
                    serviceStarted = false
                }
            }
        }
    }
}
