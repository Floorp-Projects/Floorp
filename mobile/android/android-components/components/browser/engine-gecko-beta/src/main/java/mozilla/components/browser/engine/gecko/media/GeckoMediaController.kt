/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import mozilla.components.concept.engine.media.Media
import org.mozilla.geckoview.MediaElement

/**
 * [Media.Controller] (`concept-engine`) implementation for GeckoView.
 */
internal class GeckoMediaController(
    private val mediaElement: MediaElement
) : Media.Controller {
    override fun pause() {
        mediaElement.pause()
    }

    override fun play() {
        mediaElement.play()
    }

    override fun seek(time: Double) {
        mediaElement.seek(time)
    }

    override fun setMuted(muted: Boolean) {
        mediaElement.setMuted(muted)
    }
}
