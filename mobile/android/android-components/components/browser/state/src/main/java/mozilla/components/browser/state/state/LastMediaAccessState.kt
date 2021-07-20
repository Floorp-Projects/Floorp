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
 * @property lastMediaAccess The last time media started playing in the current web document.
 * Defaults to [0] if media hasn't started playing.
 */
data class LastMediaAccessState(
    val lastMediaUrl: String = "",
    val lastMediaAccess: Long = 0
)
