/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

/**
 * Represents an add-on based on the AMO store:
 * https://addons.mozilla.org/en-US/firefox/
 *
 * @property id The unique ID of this add-on.
 * @property authors List holding information about the add-on authors.
 * @property categories List of categories the add-on belongs to.
 * @property downloadUrl The (absolute) URL to download the add-on file (eg xpi).
 * @property version The add-on version e.g "1.23.0".
 * @property permissions List of the add-on permissions for this File.
 * @property translatableName A map containing the different translations for the add-on name,
 * where the key is the language and the value is the actual translated text.
 * @property translatableDescription A map containing the different translations for the add-on description,
 * where the key is the language and the value is the actual translated text.
 * @property translatableSummary A map containing the different translations for the add-on name,
 * where the key is the language and the value is the actual translated text.
 * @property iconUrl The URL to icon for the add-on.
 * @property siteUrl The (absolute) add-on detail URL.
 * @property rating The rating information of this add-on.
 * @property createdAt The date the add-on was created.
 * @property updatedAt The date of the last time the add-on was updated by its developer(s).
 */
data class AddOn(
    val id: String,
    val authors: List<Author>,
    val categories: List<String>,
    val downloadUrl: String,
    val version: String,
    val permissions: List<String> = emptyList(),
    val translatableName: Map<String, String> = emptyMap(),
    val translatableDescription: Map<String, String> = emptyMap(),
    val translatableSummary: Map<String, String> = emptyMap(),
    val iconUrl: String = "",
    val siteUrl: String = "",
    val rating: Rating? = null,
    val createdAt: String,
    val updatedAt: String
) {
    /**
     * Represents an add-on author.
     *
     * @property id The id of the author (creator) of the add-on.
     * @property name The name of the author.
     * @property url The link to the profile page for of the author.
     * @property username The username of the author.
     */
    data class Author(
        val id: String,
        val name: String,
        val url: String,
        val username: String
    )

    /**
     * Holds all the rating information of this add-on.
     * @property average An average score from 1 to 5 of how users scored this add-on.
     * @property reviews The number of users that has scored this add-on.
     */
    data class Rating(
        val average: Float,
        val reviews: Int
    )
}
