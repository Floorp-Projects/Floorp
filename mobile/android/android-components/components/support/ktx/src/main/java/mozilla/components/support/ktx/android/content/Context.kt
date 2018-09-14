/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.app.ActivityManager
import android.content.Context

/**
 * The (visible) version name of the application, as specified by the <manifest> tag's versionName
 * attribute. E.g. "2.0".
 */
val Context.appVersionName: String?
    get() {
        val packageInfo = packageManager.getPackageInfo(packageName, 0)
        return packageInfo.versionName
    }

/**
 * Returns the handle to a system-level service by name.
 */
inline fun <reified T> Context.systemService(name: String): T {
    return getSystemService(name) as T
}

/**
 * Returns whether or not the operating system is under low memory conditions.
 */
fun Context.isOSOnLowMemory(): Boolean {
    val activityManager = systemService<ActivityManager>(Context.ACTIVITY_SERVICE)
    return ActivityManager.MemoryInfo().also { memoryInfo ->
        activityManager.getMemoryInfo(memoryInfo)
    }.lowMemory
}
