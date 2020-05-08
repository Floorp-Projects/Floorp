/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.ext

import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.tabstray.Tab

internal fun TabSessionState.toTab(mediaState: MediaState.State) = Tab(
    id,
    content.url,
    content.title,
    content.icon,
    content.thumbnail,
    when (mediaState) {
        MediaState.State.PLAYING -> Media.State.PLAYING
        MediaState.State.PAUSED -> Media.State.PAUSED
        else -> null
    }
)
