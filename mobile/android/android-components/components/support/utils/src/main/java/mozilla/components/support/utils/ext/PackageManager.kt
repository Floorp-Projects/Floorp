/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.content.Intent
import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import android.os.Build

/**
 * Get [ResolveInfo] list for an [Intent] with a specified flag
 *
 * @param intent The name of the package to check for.
 */
fun PackageManager.queryIntentActivitiesCompat(intent: Intent, flag: Int): MutableList<ResolveInfo> {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        queryIntentActivities(intent, PackageManager.ResolveInfoFlags.of(flag.toLong()))
    } else {
        @Suppress("DEPRECATION")
        queryIntentActivities(intent, flag)
    }
}

/**
 * Get [ResolveInfo] for an [Intent] with a specified flag
 *
 * @param intent The name of the package to check for.
 */
fun PackageManager.resolveActivityCompat(intent: Intent, flag: Int): ResolveInfo? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        resolveActivity(intent, PackageManager.ResolveInfoFlags.of(flag.toLong()))
    } else {
        @Suppress("DEPRECATION")
        resolveActivity(intent, flag)
    }
}

/**
 * Get a package info with a specified flag
 *
 * @param packageName The name of the package to check for.
 */
fun PackageManager.getPackageInfoCompat(packageName: String, flag: Int): PackageInfo {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getPackageInfo(packageName, PackageManager.PackageInfoFlags.of(flag.toLong()))
    } else {
        @Suppress("DEPRECATION")
        getPackageInfo(packageName, flag)
    }
}

/**
 * Get a application info with a specified flag
 *
 * @param host The URI host.
 */
fun PackageManager.getApplicationInfoCompat(host: String, flag: Int): ApplicationInfo {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getApplicationInfo(host, PackageManager.ApplicationInfoFlags.of(flag.toLong()))
    } else {
        @Suppress("DEPRECATION")
        getApplicationInfo(host, flag)
    }
}
