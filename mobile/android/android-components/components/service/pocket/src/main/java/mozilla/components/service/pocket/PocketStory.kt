/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

/**
 * A Pocket story downloaded from the Internet and intended to be displayed in the application.
 */
sealed class PocketStory {
    /**
     * Title of the story.
     */
    abstract val title: String

    /**
     * Url where the story can be full read.
     */
    abstract val url: String

    /**
     * A Pocket recommended story.
     *
     * @property title The title of the story.
     * @property url A "pocket.co" shortlink for the original story's page.
     * @property imageUrl A url to a still image representing the story.
     * @property publisher Optional publisher name/domain, e.g. "The New Yorker" / "nationalgeographic.co.uk"".
     * **May be empty**.
     * @property category Topic of interest under which similar stories are grouped.
     * @property timeToRead Inferred time needed to read the entire story. **May be -1**.
     */
    data class PocketRecommendedStory(
        override val title: String,
        override val url: String,
        val imageUrl: String,
        val publisher: String,
        val category: String,
        val timeToRead: Int,
        val timesShown: Long
    ) : PocketStory()

    /**
     * A Pocket sponsored story.
     *
     * @property title The title of the story.
     * @property url 3rd party url containing the original story.
     * @property imageUrl A url to a still image representing the story.
     * Contains a "resize" parameter in the form of "resize=w618-h310" allowing to get the image
     * with a specific resolution and the CENTER_CROP ScaleType.
     * @property sponsor 3rd party sponsor of this story, e.g. "NextAdvisor".
     * @property shim Unique identifiers for when the user interacts with this story.
     */
    data class PocketSponsoredStory(
        override val title: String,
        override val url: String,
        val imageUrl: String,
        val sponsor: String,
        val shim: PocketSponsoredStoryShim,
    ) : PocketStory()

    /**
     * Sponsored story unique identifiers intended to be used in telemetry.
     *
     * @property click Unique identifier for when the sponsored story is clicked.
     * @property impression Unique identifier for when the user sees this sponsored story.
     */
    data class PocketSponsoredStoryShim(
        val click: String,
        val impression: String,
    )
}
