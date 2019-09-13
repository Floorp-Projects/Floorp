/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.browser.state.state.content.FindResultState

/**
 * Create a matching [TabSessionState] from a [Session].
 */
fun Session.toTabSessionState(): TabSessionState {
    return TabSessionState(id, toContentState(), toTrackingProtectionState(), parentId = parentId)
}

/**
 * Creates a matching [CustomTabSessionState] from a custom tab [Session].
 */
fun Session.toCustomTabSessionState(): CustomTabSessionState {
    val config = customTabConfig ?: throw IllegalStateException("Session is not a custom tab session")
    return CustomTabSessionState(id, toContentState(), toTrackingProtectionState(), config)
}

private fun Session.toContentState(): ContentState {
    return ContentState(
        url,
        private,
        title,
        progress,
        loading,
        searchTerms,
        securityInfo.toSecurityInfoState(),
        thumbnail
    )
}

/**
 * Creates a matching [SecurityInfoState] from a [Session.SecurityInfo]
 */
fun Session.SecurityInfo.toSecurityInfoState(): SecurityInfoState {
    return SecurityInfoState(secure, host, issuer)
}

/**
 * Creates a matching [FindResultState] from a [Session.FindResult]
 */
fun Session.FindResult.toFindResultState(): FindResultState {
    return FindResultState(activeMatchOrdinal, numberOfMatches, isDoneCounting)
}

/**
 * Creates a matching [TrackingProtectionState] from a [Session].
 */
private fun Session.toTrackingProtectionState(): TrackingProtectionState = TrackingProtectionState(
    enabled = trackerBlockingEnabled,
    blockedTrackers = trackersBlocked,
    loadedTrackers = trackersLoaded
)
