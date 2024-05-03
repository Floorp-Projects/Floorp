/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.app.ActivityManager
import android.content.Context
import androidx.core.content.ContextCompat

/**
 * This class provides information about the device memory info without exposing the android
 * framework APIs directly, making it easier to test the code that depends on it.
 */
interface MemoryInfoProvider {
    /**
     * Returns the device's available memory
     */
    fun getAvailMem(): Long
}

/**
 * This class retrieves the available memory on device using activity manager.
 */
class DefaultMemoryInfoProvider(private val context: Context) : MemoryInfoProvider {
    override fun getAvailMem(): Long {
        val activityManager = ContextCompat.getSystemService(context, ActivityManager::class.java)
        val memoryInfo = ActivityManager.MemoryInfo()
        activityManager?.getMemoryInfo(memoryInfo)
        return memoryInfo.availMem
    }
}
