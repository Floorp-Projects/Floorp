/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.annotation.SuppressLint
import android.content.Context
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatActivity

/**
 * Base activity for apps that want to customized the system defined language by their own.
 */
open class LocaleAwareAppCompatActivity : AppCompatActivity() {
    @SuppressLint("AppBundleLocaleChanges")
    override fun attachBaseContext(base: Context) {
        val locale = LocaleManager.getCurrentLocale(base) ?: LocaleManager.getSystemDefault()

        // According to https://android-review.googlesource.com/c/platform/frameworks/support/+/2137592
        // We should re-apply overrides.
        val overrideConfiguration = Configuration()

        // Modify the configuration as needed
        overrideConfiguration.setLocale(locale)

        val newContext = base.createConfigurationContext(overrideConfiguration)
        val contextWrapper = ActivityContextWrapper(newContext, base)
        super.attachBaseContext(contextWrapper)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setLayoutDirectionIfNeeded()
    }

    /**
     * Compensates for a bug in Android 8 which doesn't change the layoutDirection on activity recreation
     * https://github.com/mozilla-mobile/fenix/issues/9413
     * https://stackoverflow.com/questions/46296202/rtl-layout-bug-in-android-oreo#comment98890942_46298101
     */
    @SuppressWarnings("VariableNaming")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun setLayoutDirectionIfNeeded() {
        val isAndroid8 = Build.VERSION.SDK_INT == Build.VERSION_CODES.O
        val isAndroid8_1 = Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1

        if (isAndroid8 || isAndroid8_1) {
            window.decorView.layoutDirection = resources.configuration.layoutDirection
        }
    }
}
