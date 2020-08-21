/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

/**
 * A top site.
 *
 * @property id Unique ID of this top site.
 * @property title The title of the top site.
 * @property url The URL of the top site.
 * @property createdAt The optional date the top site was added.
 * @property type The type of a top site.
 */
data class TopSite(
    val id: Long?,
    val title: String?,
    val url: String,
    val createdAt: Long?,
    val type: Type
) {
    /**
     * The type of a [TopSite].
     */
    enum class Type {
        /**
         * This top site was added as a default by the application.
         */
        DEFAULT,

        /**
         * This top site was pinned by an user.
         */
        PINNED,

        /**
         * This top site is auto-generated from the history storage based on the most frecent site.
         */
        FRECENT
    }
}
