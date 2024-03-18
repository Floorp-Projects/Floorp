/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.content.blocking

/**
 * Represents a site that will be ignored by the tracking protection policies.
 */
interface TrackingProtectionException {

    /**
     * The url of the site to be ignored.
     */
    val url: String
}
