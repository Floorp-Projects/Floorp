/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.tabstray

import android.graphics.Bitmap
import mozilla.components.concept.engine.mediasession.MediaSession

/**
 * Data class representing a tab to be displayed in a [TabsTray].
 *
 * @property id Unique ID of the tab.
 * @property url Current URL of the tab.
 * @property title Current title of the tab (or an empty [String]]).
 * @property private whether or not the session is private.
 * @property icon Current icon of the tab (or null)
 * @property thumbnail Current thumbnail of the tab (or null)
 * @property playbackState Current media session playback state for the tab (or null)
 * @property controller Current media session controller for the tab (or null)
 * @property lastAccess The last time this tab was selected.
 * @property createdAt When the tab was first created.
 * @property searchTerm the last used search term for this tab or from the originating tab, or an
 * empty string if no search was executed.
 */
@Deprecated(
    "This will be removed in a future release",
    ReplaceWith("TabSessionState", "mozilla.components.browser.state.state"),
)
data class Tab(
    val id: String,
    val url: String,
    val title: String = "",
    val private: Boolean = false,
    val icon: Bitmap? = null,
    val thumbnail: Bitmap? = null,
    val playbackState: MediaSession.PlaybackState? = null,
    val controller: MediaSession.Controller? = null,
    val lastAccess: Long = 0L,
    val createdAt: Long = 0L,
    val searchTerm: String = "",
)
