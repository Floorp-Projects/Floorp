/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import org.mozilla.fenix.Config
import org.mozilla.fenix.ext.isTablet
import org.mozilla.fenix.ext.settings

/**
 * Returns true if the tab strip is enabled.
 */
fun Context.isTabStripEnabled(): Boolean =
    isTabStripEligible() && settings().isTabStripEnabled

/**
 * Returns true if the the device has the prerequisites to enable the tab strip.
 */
fun Context.isTabStripEligible(): Boolean =
    Config.channel.isNightlyOrDebug && isTablet() && !doesDeviceHaveHinge()

/**
 * Check if the device has a hinge sensor.
 */
private fun Context.doesDeviceHaveHinge(): Boolean =
    Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
        packageManager.hasSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE)
