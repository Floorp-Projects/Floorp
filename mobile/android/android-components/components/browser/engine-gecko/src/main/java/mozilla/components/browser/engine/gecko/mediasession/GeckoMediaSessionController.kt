/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.mediasession

import mozilla.components.concept.engine.mediasession.MediaSession
import org.mozilla.geckoview.MediaSession as GeckoViewMediaSession

/**
 * [MediaSession.Controller] (`concept-engine`) implementation for GeckoView.
 */
internal class GeckoMediaSessionController(
    private val mediaSession: GeckoViewMediaSession,
) : MediaSession.Controller {

    override fun pause() {
        mediaSession.pause()
    }

    override fun stop() {
        mediaSession.stop()
    }

    override fun play() {
        mediaSession.play()
    }

    override fun seekTo(time: Double, fast: Boolean) {
        mediaSession.seekTo(time, fast)
    }

    override fun seekForward() {
        mediaSession.seekForward()
    }

    override fun seekBackward() {
        mediaSession.seekBackward()
    }

    override fun nextTrack() {
        mediaSession.nextTrack()
    }

    override fun previousTrack() {
        mediaSession.previousTrack()
    }

    override fun skipAd() {
        mediaSession.skipAd()
    }

    override fun muteAudio(mute: Boolean) {
        mediaSession.muteAudio(mute)
    }
}
