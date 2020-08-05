/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

/**
 * A pinned site.
 */
interface PinnedSite {
    /**
     * Unique ID of this pinned site.
     */
    val id: Long

    /**
     * The title of the pinned site.
     */
    val title: String

    /**
     * The URL of the pinned site.
     */
    val url: String

    /**
     * Whether or not the pinned site is a default pinned site (added as a default by the application).
     */
    val isDefault: Boolean
}
