/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.controller

import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.onboarding.interactor.OnboardingInteractor
import org.mozilla.fenix.settings.SupportUtils

/**
 * An interface that handles the view manipulation of the first run onboarding.
 */
interface OnboardingController {
    /**
     * @see [OnboardingInteractor.onStartBrowsingClicked]
     */
    fun handleStartBrowsingClicked()

    /**
     * @see [OnboardingInteractor.onReadPrivacyNoticeClicked]
     */
    fun handleReadPrivacyNoticeClicked()
}

/**
 * The default implementation of [OnboardingController].
 */
class DefaultOnboardingController(
    private val activity: HomeActivity,
    private val hideOnboarding: () -> Unit,
) : OnboardingController {

    override fun handleStartBrowsingClicked() {
        hideOnboarding()
    }

    override fun handleReadPrivacyNoticeClicked() {
        activity.startActivity(
            SupportUtils.createCustomTabIntent(
                activity,
                SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE),
            ),
        )
    }
}
