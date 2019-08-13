/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.focus

import android.media.AudioManager
import android.os.Build
import mozilla.components.feature.media.ext.getMedia
import mozilla.components.feature.media.ext.pause
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.media.ext.playIfPaused
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.support.base.log.logger.Logger

/**
 * Class responsible for request audio focus and reacting to audio focus changes.
 *
 * https://developer.android.com/guide/topics/media-apps/audio-focus
 */
internal class AudioFocus(
    val audioManager: AudioManager
) : AudioManager.OnAudioFocusChangeListener {
    private val logger = Logger("AudioFocus")
    private var playDelayed = false
    private var resumeOnFocusGain = false

    private val audioFocusController = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        AudioFocusControllerV26(audioManager, this)
    } else {
        AudioFocusControllerV21(audioManager, this)
    }

    @Synchronized
    fun request(state: MediaState) {
        val result = audioFocusController.request()
        processAudioFocusResult(state, result)
    }

    @Synchronized
    fun abandon() {
        audioFocusController.abandon()
        playDelayed = false
        resumeOnFocusGain = false
    }

    private fun processAudioFocusResult(state: MediaState, result: Int) {
        logger.debug("processAudioFocusResult($result)")

        when (result) {
            AudioManager.AUDIOFOCUS_REQUEST_GRANTED -> {
                // Granted: Gecko already started playing media.
                playDelayed = false
                resumeOnFocusGain = false
            }
            AudioManager.AUDIOFOCUS_REQUEST_FAILED -> {
                // Failed: Pause media since we didn't get audio focus.
                state.getMedia().pause()
                playDelayed = false
                resumeOnFocusGain = false
            }
            AudioManager.AUDIOFOCUS_REQUEST_DELAYED -> {
                // Delayed: Pause media until we gain focus via callback
                state.getMedia().pause()
                playDelayed = true
                resumeOnFocusGain = false
            }
            else -> throw IllegalStateException("Unknown audio focus request response: $result")
        }
    }

    @Synchronized
    override fun onAudioFocusChange(focusChange: Int) {
        logger.debug("onAudioFocusChange($focusChange)")

        val state = MediaStateMachine.state

        when (focusChange) {
            AudioManager.AUDIOFOCUS_GAIN -> {
                if (playDelayed || resumeOnFocusGain) {
                    state.playIfPaused()
                    playDelayed = false
                    resumeOnFocusGain = false
                }
            }

            AudioManager.AUDIOFOCUS_LOSS -> {
                state.pauseIfPlaying()
                resumeOnFocusGain = false
                playDelayed = false
            }

            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> {
                resumeOnFocusGain = true
                playDelayed = false
                state.pauseIfPlaying()
            }

            else -> {
                logger.debug("Unhandled focus change: $focusChange")
            }

            // We do not handle any ducking related focus change here. On API 26+ the system should
            // duck and restore the volume automatically
            // https://github.com/mozilla-mobile/android-components/issues/3936
        }
    }
}
