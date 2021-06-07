/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.concept.engine.EngineSession

/**
 * Create a matching [TabSessionState] from a [Session].
 */
fun Session.toTabSessionState(
    initialLoadFlags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none()
): TabSessionState {
    return TabSessionState(
        id,
        toContentState(),
        toTrackingProtectionState(),
        parentId = parentId,
        contextId = contextId,
        source = source,
        engineState = EngineState(initialLoadFlags = initialLoadFlags)
    )
}

/**
 * Creates a matching [CustomTabSessionState] from a custom tab [Session].
 */
fun Session.toCustomTabSessionState(
    initialLoadFlags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none()
): CustomTabSessionState {
    val config =
        customTabConfig ?: throw IllegalStateException("Session is not a custom tab session")
    return CustomTabSessionState(
        id,
        toContentState(),
        toTrackingProtectionState(),
        config,
        contextId = contextId,
        engineState = EngineState(initialLoadFlags = initialLoadFlags)
    )
}

private fun Session.toContentState(): ContentState {
    return ContentState(
        url,
        private,
        title,
        progress,
        loading,
        webAppManifest = webAppManifest,
        securityInfo = securityInfo.toSecurityInfoState()
    )
}

/**
 * Creates a matching [SecurityInfoState] from a [Session.SecurityInfo]
 */
fun Session.SecurityInfo.toSecurityInfoState(): SecurityInfoState {
    return SecurityInfoState(secure, host, issuer)
}

/**
 * Creates a matching [TrackingProtectionState] from a [Session].
 */
private fun Session.toTrackingProtectionState(): TrackingProtectionState = TrackingProtectionState(
    enabled = trackerBlockingEnabled,
    blockedTrackers = trackersBlocked,
    loadedTrackers = trackersLoaded
)
