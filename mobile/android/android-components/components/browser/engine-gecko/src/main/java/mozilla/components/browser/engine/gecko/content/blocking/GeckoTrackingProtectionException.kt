/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.content.blocking

import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission

/**
 * Represents a site that will be ignored by the tracking protection policies.
 * @property url The url of the site to be ignored.
 * @property privateMode Indicates if this exception should persisted in private mode.
 * @property contentPermission The associated gecko content permission of this exception.
 */
data class GeckoTrackingProtectionException(
    override val url: String,
    val privateMode: Boolean = false,
    val contentPermission: ContentPermission,
) : TrackingProtectionException
