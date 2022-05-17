/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

/**
 * A Pocket recommended story.
 *
 * @property title the title of the story.
 * @property url a "pocket.co" shortlink for the original story's page.
 * @property imageUrl a url to a still image representing the story.
 * @property publisher optional publisher name/domain, e.g. "The New Yorker" / "nationalgeographic.co.uk"".
 * **May be empty**.
 * @property category topic of interest under which similar stories are grouped.
 * @property timeToRead inferred time needed to read the entire story. **May be -1**.
 */
data class PocketRecommendedStory(
    val title: String,
    val url: String,
    val imageUrl: String,
    val publisher: String,
    val category: String,
    val timeToRead: Int,
    val timesShown: Long
)
