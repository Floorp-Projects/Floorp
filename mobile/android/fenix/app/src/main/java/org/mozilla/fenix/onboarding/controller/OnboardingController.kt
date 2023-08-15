/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.controller

import androidx.navigation.NavController
import mozilla.components.lib.crash.CrashReporter
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.ext.navigateWithBreadcrumb
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.onboarding.OnboardingFragmentDirections
import org.mozilla.fenix.onboarding.interactor.OnboardingInteractor
import org.mozilla.fenix.settings.SupportUtils

/**
 * An interface that handles the view manipulation of the first run onboarding.
 */
interface OnboardingController {
    /**
     * @see [OnboardingInteractor.onFinishOnboarding]
     */
    fun handleFinishOnboarding(focusOnAddressBar: Boolean)

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
    private val navController: NavController,
    private val onboarding: FenixOnboarding,
    private val crashReporter: CrashReporter,
) : OnboardingController {

    override fun handleFinishOnboarding(focusOnAddressBar: Boolean) {
        onboarding.finish()

        navController.navigateWithBreadcrumb(
            directions = OnboardingFragmentDirections.actionHome(focusOnAddressBar = focusOnAddressBar),
            navigateFrom = "OnboardingFragment",
            navigateTo = "ActionHome",
            crashReporter = crashReporter,
        )

        if (focusOnAddressBar) {
            Events.searchBarTapped.record(Events.SearchBarTappedExtra("HOME"))
        }
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
