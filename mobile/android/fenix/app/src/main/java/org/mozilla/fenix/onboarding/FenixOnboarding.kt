/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import android.content.SharedPreferences
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components

class FenixOnboarding(private val context: Context) {
    private val onboardingPrefs = context.applicationContext.getSharedPreferences(
        OnboardingKeys.PREF_NAME.key,
        Context.MODE_PRIVATE
    )

    private var SharedPreferences.onboardedVersion: Int
        get() = getInt(OnboardingKeys.LAST_VERSION.key, 0)
        set(version) { edit().putInt(OnboardingKeys.LAST_VERSION.key, version).apply() }

    fun finish() {
        onboardingPrefs.onboardedVersion = CURRENT_ONBOARDING_VERSION
        context.components.analytics.metrics.track(Event.DismissedOnboarding)
    }

    fun userHasBeenOnboarded(): Boolean {
        return onboardingPrefs.onboardedVersion == CURRENT_ONBOARDING_VERSION
    }

    private enum class OnboardingKeys(val key: String) {
        PREF_NAME("fenix.onboarding"),
        LAST_VERSION("fenix.onboarding.last_version")
    }

    companion object {
        private const val CURRENT_ONBOARDING_VERSION = 1
    }
}
