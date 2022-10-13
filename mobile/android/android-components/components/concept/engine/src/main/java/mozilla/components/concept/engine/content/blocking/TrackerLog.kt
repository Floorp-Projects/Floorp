/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.content.blocking

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory

/**
 * Represents a blocked content tracker.
 * @property url The URL of the tracker.
 * @property loadedCategories A list of tracking categories loaded for this tracker.
 * @property blockedCategories A list of tracking categories blocked for this tracker.
 * @property unBlockedBySmartBlock Indicates if the content of the [blockedCategories]
 * has been partially unblocked by the SmartBlock feature.
 */
data class TrackerLog(
    val url: String,
    val loadedCategories: List<TrackingCategory> = emptyList(),
    val blockedCategories: List<TrackingCategory> = emptyList(),
    val cookiesHasBeenBlocked: Boolean = false,
    val unBlockedBySmartBlock: Boolean = false,
)
