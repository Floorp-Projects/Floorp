/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import androidx.compose.runtime.Composable
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.ext.toShortUrl

/**
 * Shortens URLs to be more user friendly, by applying [String.toShortUrl]
 * and making sure it's equal or below the [MAX_URI_LENGTH].
 */
@Composable
fun TabSessionState.toShortUrl(): String {
    // Truncate to MAX_URI_LENGTH to prevent the UI from locking up for
    // extremely large URLs such as data URIs or bookmarklets. The same
    // is done in the toolbar and awesomebar:
    // https://github.com/mozilla-mobile/fenix/issues/1824
    // https://github.com/mozilla-mobile/android-components/issues/6985
    return if (inComposePreview) {
        this.content.url.take(MAX_URI_LENGTH)
    } else {
        this.content.url.toShortUrl(components.publicSuffixList)
            .take(MAX_URI_LENGTH)
    }
}

fun TabSessionState.isActive(maxActiveTime: Long): Boolean {
    val lastActiveTime = maxOf(lastAccess, createdAt)
    val now = System.currentTimeMillis()
    return (now - lastActiveTime <= maxActiveTime)
}

/**
 * Returns true if the [TabSessionState] has a search term.
 */
fun TabSessionState.hasSearchTerm(): Boolean {
    return content.searchTerms.isNotEmpty() || !historyMetadata?.searchTerm.isNullOrBlank()
}

/**
 * Returns true if the [TabSessionState] is considered active based on the [maxActiveTime].
 */
internal fun TabSessionState.isNormalTabActive(maxActiveTime: Long): Boolean {
    return isActive(maxActiveTime) && !content.private
}

/**
 * Returns true if the [TabSessionState] have a search term.
 */
internal fun TabSessionState.isNormalTabActiveWithSearchTerm(maxActiveTime: Long): Boolean {
    return isNormalTabActive(maxActiveTime) && hasSearchTerm()
}

/**
 * Returns true if the [TabSessionState] has a search term but may or may not be active.
 */
internal fun TabSessionState.isNormalTabWithSearchTerm(): Boolean {
    return hasSearchTerm() && !content.private
}

/**
 * Returns true if the [TabSessionState] is considered active based on the [maxActiveTime].
 */
internal fun TabSessionState.isNormalTabInactive(maxActiveTime: Long): Boolean {
    return !isActive(maxActiveTime) && !content.private
}

/**
 * Returns true if the [TabSessionState] is not private.
 */
internal fun TabSessionState.isNormalTab(): Boolean {
    return !content.private
}
