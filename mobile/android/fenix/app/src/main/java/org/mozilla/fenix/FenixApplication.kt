/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Application
import org.mozilla.fenix.components.Components

class FenixApplication : Application() {
    val components by lazy { Components(this) }

    override fun onCreate() {
        super.onCreate()

        setupCrashReporting()
    }

    private fun setupCrashReporting() {
        @Suppress("ConstantConditionIf")
        if (!BuildConfig.CRASH_REPORTING || BuildConfig.BUILD_TYPE != "release") {
            // Only enable crash reporting if this is a release build and if crash reporting was explicitly enabled
            // via a Gradle command line flag.
            return
        }

        components
            .analytics
            .crashReporter
            .install(this)
    }
}
