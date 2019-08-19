/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import org.mozilla.fenix.ext.components

class FenixOnboarding(context: Context) {
    private val metrics = context.components.analytics.metrics
    private val onboardingPrefs = context.getSharedPreferences(
        PREF_NAME_ONBOARDING_KEY,
        Context.MODE_PRIVATE
    )

    private var SharedPreferences.onboardedVersion: Int
        get() = getInt(LAST_VERSION_ONBOARDING_KEY, 0)
        set(version) { edit { putInt(LAST_VERSION_ONBOARDING_KEY, version) } }

    fun finish() {
        onboardingPrefs.onboardedVersion = CURRENT_ONBOARDING_VERSION

        // To be fixed in #4824
        // metrics.track(Event.DismissedOnboarding)
    }

    fun userHasBeenOnboarded() = onboardingPrefs.onboardedVersion == CURRENT_ONBOARDING_VERSION

    companion object {
        private const val CURRENT_ONBOARDING_VERSION = 1

        private const val PREF_NAME_ONBOARDING_KEY = "fenix.onboarding"
        private const val LAST_VERSION_ONBOARDING_KEY = "fenix.onboarding.last_version"
    }
}
