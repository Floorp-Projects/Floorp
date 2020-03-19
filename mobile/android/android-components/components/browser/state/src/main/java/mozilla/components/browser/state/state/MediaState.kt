/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.media.Media.Controller
import mozilla.components.concept.engine.media.Media.PlaybackState
import java.util.UUID

/**
 * Value type that represents the state of the media elements and playback states.
 *
 * @property aggregate The "global" aggregated media state.
 * @property elements A map from tab ids to a list of media elements on that tab.
 */
data class MediaState(
    val aggregate: Aggregate = Aggregate(),
    val elements: Map<String, List<Element>> = emptyMap()
) {
    /**
     * Enum of "global" aggregated media state.
     */
    enum class State {
        NONE,
        PLAYING,
        PAUSED
    }

    /**
     * Value type representing a media element on a website.
     *
     * @property id Unique ID for this media element.
     * @property state The current simplified [State] of this media element (derived from [playbackState]
     * events).
     * @property playbackState The current [PlaybackState] of this media element.
     * @property controller The [Controller] for controlling playback of this media element.
     * @property metadata The [Metadata] for this media element.
     */
    data class Element(
        val id: String = UUID.randomUUID().toString(),
        val state: Media.State,
        val playbackState: PlaybackState,
        val controller: Controller,
        val metadata: Media.Metadata
    )

    /**
     * Value type representing the aggregated "global" media state.
     *
     * @property state The aggregated [State] for all tabs and media combined.
     * @property activeTabId The ID of the currently active tab (or null). Can be the id of a regular
     * tab or a custom tab.
     * @property activeMedia A list of ids (String) of currently active media elements on the active
     * tab.
     */
    data class Aggregate(
        val state: State = State.NONE,
        val activeTabId: String? = null,
        val activeMedia: List<String> = emptyList()
    )
}
