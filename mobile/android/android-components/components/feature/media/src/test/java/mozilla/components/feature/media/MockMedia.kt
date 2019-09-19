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