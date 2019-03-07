/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.data

/**
 * A recommended video as returned from the Pocket Global Video Recommendation endpoint v2.
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

    data class Author(
        val id: String,
        val name: String,
        val url: String
    )
}
