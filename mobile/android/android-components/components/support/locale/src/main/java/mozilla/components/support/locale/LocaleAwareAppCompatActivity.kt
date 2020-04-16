/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.content.Context
import android.os.Build
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatActivity

/**
 * Base activity for apps that want to customized the system defined language by their own.
 */
open class LocaleAwareAppCompatActivity : AppCompatActivity() {
    override fun attachBaseContext(base: Context) {
        val context = LocaleManager.updateResources(base)
        super.attachBaseContext(context)
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
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun setLayoutDirectionIfNeeded() {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.O) {
            window.decorView.layoutDirection = resources.configuration.layoutDirection
        }
    }
}
