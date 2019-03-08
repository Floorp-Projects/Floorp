/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.data

/**
 * A recommended video as returned from the Pocket Global Video Recommendation endpoint v2.
 *
 * @property id a unique identifier for this recommendation.
 * @property url a url to the video.
 * @property tvURL a url to the video on pages formatted for TV form factors (e.g. YouTube.com/tv).
 * @property title the title of the video.
 * @property excerpt a summary of the video.
 * @property domain the domain where the video appears, e.g. "youtube.com".
 * @property imageSrc a url to a still image representing the video.
 * @property publishedTimestamp unknown: please ask for clarification if needed. This may be "0".
 * @property sortId the index of this recommendation in the list which is sorted by date added to the API results.
 * @property popularitySortId the index of this recommendation in the list if the list was sorted by popularity.
 * @property authors the authors or publishers of this recommendation; unclear if this can be empty.
 */
data class PocketGlobalVideoRecommendation(
    val id: Int,
    val url: String,
    val tvURL: String,
    val title: String,
    val excerpt: String,
    val domain: String,
    val imageSrc: String,
    val publishedTimestamp: String,
    val sortId: Int,
    val popularitySortId: Int,
    val authors: List<Author>
) {

    /**
     * An author or publisher of a [PocketGlobalVideoRecommendation].
     *
     * @property id a unique identifier for this author.
     * @property name the name of this author.
     * @property url a url to the author's video content channel (e.g. a YouTube channel).
     */
    data class Author(
        val id: String,
        val name: String,
        val url: String
    )
}
