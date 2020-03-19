/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.session

import android.support.v4.media.session.MediaSessionCompat
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.media.ext.playIfPaused
import mozilla.components.support.base.log.logger.Logger

internal class MediaSessionCallback(
    private val store: BrowserStore
) : MediaSessionCompat.Callback() {
    private val logger = Logger("MediaSessionCallback")

    override fun onPlay() {
        logger.debug("play()")

        store
            .state
            .media
            .playIfPaused()
    }

    override fun onPause() {
        logger.debug("pause()")

        store
            .state
            .media
            .pauseIfPlaying()
    }
}
