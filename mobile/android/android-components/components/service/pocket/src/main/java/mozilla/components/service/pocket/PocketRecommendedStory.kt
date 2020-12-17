/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

/**
 * A Pocket recommended story.
 *
 * @property id a unique identifier for this recommendation.
 * @property url a "pocket.co" shortlink for the original story's page.
 * @property domain the domain where the story appears, e.g. "bbc.com".
 * @property title the title of the story.
 * @property excerpt a summary of the story.
 * @property imageSrc a url to a still image representing the story.
 * @property publishedTimestamp story publish date. May have a negative value if the date is unknown.
 * @property dedupeUrl the full url to the story's page. Will contain "?utm_source=pocket-newtab".
 * @property sortId the index of this recommendation in the list which is sorted by date added to the API results.
 */
data class PocketRecommendedStory(
    val id: Long,
    val url: String,
    val domain: String,
    val title: String,
    val excerpt: String,
    val imageSrc: String,
    val publishedTimestamp: String,
    val dedupeUrl: String,
    val sortId: Int
)
