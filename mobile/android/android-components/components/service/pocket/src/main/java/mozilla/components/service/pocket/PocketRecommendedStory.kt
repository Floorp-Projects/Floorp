/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

/**
 * A Pocket recommended story.
 *
 * @property id a unique identifier for this recommendation.
 * @property title the title of the story.
 * @property publisher the name domain of the publisher, e.g. "The New Yorker" / "nationalgeographic.co.uk"".
 * @property url a "pocket.co" shortlink for the original story's page.
 * @property imageUrl a url to a still image representing the story.
 * @property timeToRead inferred time needed to read the entire story.
 * @property category topic of interest under which similar stories are grouped.
 */
data class PocketRecommendedStory(
    val title: String,
    val publisher: String,
    val url: String,
    val imageUrl: String,
    val timeToRead: Int,
    val category: String
)
