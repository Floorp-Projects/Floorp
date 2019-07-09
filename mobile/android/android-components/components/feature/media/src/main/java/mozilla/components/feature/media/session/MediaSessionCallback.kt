/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.session

import android.support.v4.media.session.MediaSessionCompat
import mozilla.components.feature.media.ext.getMedia
import mozilla.components.feature.media.ext.pause
import mozilla.components.feature.media.ext.play
import mozilla.components.feature.media.notification.MediaNotificationFeature
import mozilla.components.support.base.log.logger.Logger

internal class MediaSessionCallback : MediaSessionCompat.Callback() {
    private val logger = Logger("MediaSessionCallback")

    override fun onPlay() {
        logger.debug("play()")

        MediaNotificationFeature
            .getState()
            .getMedia()
            .play()
    }

    override fun onPause() {
        logger.debug("pause()")

        MediaNotificationFeature
            .getState()
            .getMedia()
            .pause()
    }
}
