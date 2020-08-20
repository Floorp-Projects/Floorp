/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

class OnboardingInteractor(private val onboardingController: OnboardingController) {

    /**
     * Called when the user clicks the learn more link
     * @param url the url the suggestion was providing
     */
    fun onLearnMoreClicked() = onboardingController.handleLearnMoreClicked()
}
