/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import mozilla.components.concept.engine.media.Media
import mozilla.components.support.test.mock

internal class MockMedia(
    initialState: PlaybackState
) : Media() {
    init {
        playbackState = initialState
    }

    override val controller: Controller =
        mock()
    override val metadata: Metadata =
        mock()
}
