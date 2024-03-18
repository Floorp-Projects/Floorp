/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * Details about the last playing media in this tab.
 *
 * @property lastMediaUrl - [ContentState.url] when media started playing.
 * This is not the URL of the media but of the page when media started.
 * Defaults to "" (an empty String) if media hasn't started playing.
 * This value is only updated when media starts playing.
 * Can be used as a backup to [mediaSessionActive] for knowing the user is still on the same website
 * on which media was playing before media started playing in another tab.
 *
 * @property lastMediaAccess The last time media started playing in the current web document.
 * Defaults to [0] if media hasn't started playing.
 * This value is only updated when media starts playing.
 *
 * @property mediaSessionActive Whether or not the last accessed media is still active.
 * Can be used as a backup to [lastMediaUrl] on websites which allow media to continue playing
 * even when the users accesses another page (with another URL) in that same HTML document.
 */
data class LastMediaAccessState(
    val lastMediaUrl: String = "",
    val lastMediaAccess: Long = 0,
    val mediaSessionActive: Boolean = false,
)
