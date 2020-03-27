/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware.sideeffects

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.core.content.ContextCompat
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.media.service.AbstractMediaService

/**
 * Takes care of launching a media service, implementing [AbstractMediaService], if needed.
 */
internal class MediaServiceLauncher(
    private val context: Context,
    private val mediaServiceClass: Class<*>
) {
    fun process(state: BrowserState) {
        if (state.media.aggregate.state == MediaState.State.PLAYING) {
            launch()
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun launch() {
        ContextCompat.startForegroundService(
            context,
            AbstractMediaService.launchIntent(
                context,
                mediaServiceClass
            )
        )
    }
}
