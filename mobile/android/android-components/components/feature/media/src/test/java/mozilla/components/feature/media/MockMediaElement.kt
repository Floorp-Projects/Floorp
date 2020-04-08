/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import mozilla.components.browser.state.state.MediaState
import mozilla.components.concept.engine.media.Media
import mozilla.components.support.test.mock
import java.util.UUID

fun createMockMediaElement(
    id: String = UUID.randomUUID().toString(),
    state: Media.State = Media.State.PLAYING,
    metadata: Media.Metadata = Media.Metadata(),
    volume: Media.Volume = Media.Volume()
): MediaState.Element {
    return MediaState.Element(
        id = id,
        state = state,
        playbackState = Media.PlaybackState.PLAYING,
        controller = mock(),
        metadata = metadata,
        volume = volume
    )
}
