/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.ext

import android.content.Context
import mozilla.components.feature.sitepermissions.SitePermissions
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.utils.Settings

fun PhoneFeature.shouldBeVisible(
    sitePermissions: SitePermissions?,
    settings: Settings
) = getStatus(sitePermissions, settings) != SitePermissions.Status.NO_DECISION

fun PhoneFeature.shouldBeEnabled(
    context: Context,
    sitePermissions: SitePermissions?,
    settings: Settings
) = isAndroidPermissionGranted(context) && isUserPermissionGranted(sitePermissions, settings)

fun PhoneFeature.isUserPermissionGranted(
    sitePermissions: SitePermissions?,
    settings: Settings
) = getStatus(sitePermissions, settings) == SitePermissions.Status.ALLOWED
