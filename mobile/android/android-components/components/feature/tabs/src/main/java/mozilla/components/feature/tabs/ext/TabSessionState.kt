/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.ext

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.tabs.tabstray.Tab

internal fun TabSessionState.toTab() = Tab(
    id,
    content.url,
    content.title,
    content.private,
    content.icon,
    content.thumbnail,
    mediaSessionState?.playbackState,
    mediaSessionState?.controller,
    lastAccess,
    createdAt,
    if (content.searchTerms.isNotEmpty()) content.searchTerms else historyMetadata?.searchTerm ?: "",
)

/**
 * Check whether this tab has played media before - any media which started playing in this HTML document,
 * irrespective of it's current state (eg: playing, paused, stopped).
 */
fun TabSessionState.hasMediaPlayed(): Boolean {
    return lastMediaAccessState.lastMediaUrl == content.url || lastMediaAccessState.mediaSessionActive
}
