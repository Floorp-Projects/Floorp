/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.content.blocking

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory

/**
 * Represents a blocked content tracker.
 * @property url The URL of the tracker.
 * @property trackingCategories The anti-tracking category types of the blocked resource.
 * @property cookiePolicies The cookie types of the blocked resource.
 */
class Tracker(
    val url: String,
    val trackingCategories: List<TrackingCategory> = emptyList(),
    val cookiePolicies: List<CookiePolicy> = emptyList(),
)
