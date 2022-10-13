/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_TRACKING

/**
 * Indicates if this Gecko permission is a tracking protection permission and it is excluded
 * from the tracking protection policies.
 */
val ContentPermission.isExcludedForTrackingProtection: Boolean
    get() = this.permission == PERMISSION_TRACKING &&
        value == VALUE_ALLOW

/**
 * Provides the tracking protection permission for the given [GeckoEngineSession].
 * This is available after every onLocationChange call.
 */
val GeckoEngineSession.geckoTrackingProtectionPermission: ContentPermission?
    get() = this.geckoPermissions.find { it.permission == PERMISSION_TRACKING }
