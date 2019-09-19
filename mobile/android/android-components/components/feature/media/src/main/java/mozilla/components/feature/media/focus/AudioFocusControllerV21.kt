/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.focus

import android.media.AudioManager

/**
 * [AudioFocusController] implementation for Android API 21+.
 */
@Suppress("DEPRECATION")
internal class AudioFocusControllerV21(
    private val audioManager: AudioManager,
    private val listener: AudioManager.OnAudioFocusChangeListener
) : AudioFocusController {
    override fun request(): Int {
        return audioManager.requestAudioFocus(
            listener,
            AudioManager.STREAM_MUSIC,
            AudioManager.AUDIOFOCUS_GAIN
        )
    }

    override fun abandon() {
        audioManager.abandonAudioFocus(listener)
    }
}
