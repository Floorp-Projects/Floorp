/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import mozilla.components.browser.state.state.MediaState
import mozilla.components.concept.engine.media.Media

internal fun Media.toElement() = MediaState.Element(
    state = state,
    playbackState = playbackState,
    controller = controller,
    metadata = metadata,
    volume = volume,
    fullscreen = fullscreen
)
