/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import android.content.SharedPreferences
import android.os.StrictMode
import androidx.annotation.VisibleForTesting
import mozilla.components.support.ktx.android.content.PreferencesHolder
import mozilla.components.support.ktx.android.content.intPreference
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.nimbus.FxNimbus

class FenixOnboarding(context: Context) : PreferencesHolder {

    private val strictMode = context.components.strictMode
    private val settings = context.settings()

    override val preferences: SharedPreferences = strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
        context.getSharedPreferences(
            PREF_NAME_ONBOARDING_KEY,
            Context.MODE_PRIVATE,
        )
    }

    private var onboardedVersion by intPreference(LAST_VERSION_ONBOARDING_KEY, default = 0)

    // The onboarding configuration is retrieved lazily because:
    // - We do not want to record exposure if a user is not encountering onboarding
    // - We would like to evaluate the configuration only once (and thus it's kept in memory
    // and not re-evaluated)
    val config by lazy {
        FxNimbus.features.onboarding.recordExposure()
        FxNimbus.features.onboarding.value()
    }

    fun finish() {
        // New users that goes through the first run onboarding do not need to see the home
        // onboarding dialog.
        settings.showHomeOnboardingDialog = false

        onboardedVersion = CURRENT_ONBOARDING_VERSION
    }

    fun userHasBeenOnboarded(): Boolean {
        return strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            onboardedVersion == CURRENT_ONBOARDING_VERSION
        }
    }

    companion object {
        /**
         * The current onboarding version. When incremented,
         * users who were previously onboarded will be show the onboarding again.
         */
        @VisibleForTesting
        internal const val CURRENT_ONBOARDING_VERSION = 1

        /**
         * Name of the shared preferences file.
         */
        private const val PREF_NAME_ONBOARDING_KEY = "fenix.onboarding"

        /**
         * Key for [onboardedVersion].
         */
        @VisibleForTesting
        internal const val LAST_VERSION_ONBOARDING_KEY = "fenix.onboarding.last_version"
    }
}
