/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.content.blocking.Tracker

/**
 * Value type that represents the state of tracking protection within a [SessionState].
 *
 * @property enabled Whether tracking protection is enabled or not for this [SessionState].
 * @property blockedTrackers List of trackers that have been blocked for the currently loaded site.
 * @property loadedTrackers List of trackers that have been loaded (not blocked) for the currently
 * loaded site.
 * @property ignoredOnTrackingProtection Whether tracking protection should be enabled or not for
 * this [SessionState]
 */
data class TrackingProtectionState(
    val enabled: Boolean = false,
    val blockedTrackers: List<Tracker> = emptyList(),
    val loadedTrackers: List<Tracker> = emptyList(),
    val ignoredOnTrackingProtection: Boolean = false,
)
