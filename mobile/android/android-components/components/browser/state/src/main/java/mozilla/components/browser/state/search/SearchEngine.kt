/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.search

import android.graphics.Bitmap

/**
 * A data class representing a search engine.
 *
 * @property id the ID of this search engine.
 * @property name the name of this search engine.
 * @property icon the icon of this search engine.
 * @property type the type of this search engine.
 * @property resultUrls the list of the queried suggestions result urls.
 * @property suggestUrl the search suggestion url.
 */
data class SearchEngine(
    val id: String,
    val name: String,
    val icon: Bitmap,
    val type: Type,
    val resultUrls: List<String> = emptyList(),
    val suggestUrl: String? = null
) {
    /**
     * A enum class representing a search engine type.
     */
    enum class Type {
        BUNDLED,
        CUSTOM
    }
}
