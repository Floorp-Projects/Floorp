/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.onboarding

import android.content.Intent
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultLauncher

interface OnboardingInteractor {
    fun onFinishOnBoarding()
    fun onGetStartedButtonClicked()
    fun onMakeFocusDefaultBrowserButtonClicked(activityResultLauncher: ActivityResultLauncher<Intent>)
    fun onActivityResultImplementation(activityResult: ActivityResult)
}

class DefaultOnboardingInteractor(private val controller: OnboardingController) : OnboardingInteractor {
    override fun onFinishOnBoarding() {
        controller.handleFinishOnBoarding()
    }

    override fun onGetStartedButtonClicked() {
        controller.handleGetStartedButtonClicked()
    }

    override fun onMakeFocusDefaultBrowserButtonClicked(activityResultLauncher: ActivityResultLauncher<Intent>) {
        controller.handleMakeFocusDefaultBrowserButtonClicked(activityResultLauncher)
    }

    override fun onActivityResultImplementation(activityResult: ActivityResult) {
        controller.handleActivityResultImplementation(activityResult)
    }
}
