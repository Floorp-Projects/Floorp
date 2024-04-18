/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import android.content.Context
import android.os.Build
import android.os.PowerManager
import androidx.core.content.ContextCompat

/**
 * This class provides information about battery optimisations without exposing the android
 * framework APIs directly, making it easier to test the code that depends on it.
 */
interface PowerManagerInfoProvider {

    /**
     * Returns true if the user has disabled battery optimisations for the app.
     */
    fun isIgnoringBatteryOptimizations(): Boolean
}

/**
 * @see PowerManagerInfoProvider
 */
class DefaultPowerManagerInfoProvider(private val context: Context) : PowerManagerInfoProvider {

    private val powerManager by lazy {
        ContextCompat.getSystemService(context, PowerManager::class.java)
    }

    override fun isIgnoringBatteryOptimizations(): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            powerManager?.isIgnoringBatteryOptimizations(context.packageName) ?: false
        } else {
            true
        }
}
