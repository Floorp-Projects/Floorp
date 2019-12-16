/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

/**
 * A top site.
 */
interface TopSite {
    /**
     * Unique ID of this top site.
     */
    val id: Long

    /**
     * The title of the top site.
     */
    val title: String

    /**
     * The URL of the top site.
     */
    val url: String
}
