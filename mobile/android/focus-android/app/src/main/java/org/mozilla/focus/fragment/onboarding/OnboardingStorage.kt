/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.onboarding

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.preference.PreferenceManager
import org.mozilla.focus.R

class OnboardingStorage(val context: Context) {

    /**
     * Saves the step of the onBoarding flow.
     * If the user closes the app at step two he should start again from step two
     * when he enters again in the app.
     *
     * @param onBoardingStep current onBoarding step
     */
    internal fun saveCurrentOnboardingStepInSharePref(
        onBoardingStep: OnboardingStep,
    ) {
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
        with(sharedPref.edit()) {
            putString(
                context.getString(R.string.pref_key_onboarding_step),
                context.getString(onBoardingStep.prefId),
            )
            apply()
        }
    }

    @VisibleForTesting
    internal fun getCurrentOnboardingStepFromSharedPref(): String {
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
        return sharedPref.getString(context.getString(R.string.pref_key_onboarding_step), "")
            ?: ""
    }

    internal fun getCurrentOnboardingStep() =
        when (getCurrentOnboardingStepFromSharedPref()) {
            context.getString(R.string.pref_key_first_screen) -> OnboardingStep.ON_BOARDING_FIRST_SCREEN
            context.getString(R.string.pref_key_second_screen) -> OnboardingStep.ON_BOARDING_SECOND_SCREEN
            else -> {
                OnboardingStep.ON_BOARDING_FIRST_SCREEN
            }
        }
}
