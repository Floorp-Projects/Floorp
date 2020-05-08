/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.tabstray

import android.graphics.Bitmap
import mozilla.components.concept.engine.media.Media

/**
 * Data class representing a tab to be displayed in a [TabsTray].
 *
 * @property id Unique ID of the tab.
 * @property url Current URL of the tab.
 * @property title Current title of the tab (or an empty [String]]).
 * @property icon Current icon of the tab (or null)
 * @property thumbnail Current thumbnail of the tab (or null)
 * @property mediaState Current media state for the tab (or null)
 */
data class Tab(
    val id: String,
    val url: String,
    val title: String = "",
    val icon: Bitmap? = null,
    val thumbnail: Bitmap? = null,
    val mediaState: Media.State? = null
)
