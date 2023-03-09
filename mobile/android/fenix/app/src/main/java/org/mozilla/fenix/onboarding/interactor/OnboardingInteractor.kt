/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.interactor

import org.mozilla.fenix.onboarding.controller.OnboardingController

/**
 * Interface for onboarding related actions.
 */
interface OnboardingInteractor {
    /**
     * Hides the onboarding and navigates to Search. Called when a user clicks on the "Start Browsing" button.
     */
    fun onStartBrowsingClicked()

    /**
     * Opens a custom tab to privacy notice url. Called when a user clicks on the "read our privacy notice" button.
     */
    fun onReadPrivacyNoticeClicked()
}

/**
 * The default implementation of [OnboardingInteractor].
 *
 * @param controller An instance of [OnboardingController] which will be delegated for all user
 * interactions.
 */
class DefaultOnboardingInteractor(
    private val controller: OnboardingController,
) : OnboardingInteractor {

    override fun onStartBrowsingClicked() {
        controller.handleStartBrowsingClicked()
    }

    override fun onReadPrivacyNoticeClicked() {
        controller.handleReadPrivacyNoticeClicked()
    }
}
