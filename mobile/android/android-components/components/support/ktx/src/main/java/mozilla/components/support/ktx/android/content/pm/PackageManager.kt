/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content.pm

import android.content.pm.PackageManager

/**
 * Check if a package is installed
 *
 * @param packageName The name of the package to check for.
 */
fun PackageManager.isPackageInstalled(packageName: String): Boolean {
    return try {
        // Turn off all the flags since we don't need the return value
        getPackageInfo(packageName, 0)
        true
    } catch (e: PackageManager.NameNotFoundException) {
        false
    }
}
