/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.relaunchCleanApp
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the onboarding feature for users upgrading from a version older than 106.
 *  Note: This involves setting the feature flag On for the onboarding cards
 *
 */
class UpgradingUsersOnboardingTest {

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityIntentTestRule(isHomeOnboardingDialogEnabled = true),
    ) { it.activity }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1913592
    @Test
    fun upgradingUsersOnboardingScreensTest() {
        homeScreen {
            verifyUpgradingUserOnboardingFirstScreen(activityTestRule)
            clickGetStartedButton(activityTestRule)
            verifyUpgradingUserOnboardingSecondScreen(activityTestRule)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1913591
    @Test
    fun upgradingUsersOnboardingCanBeSkippedTest() {
        homeScreen {
            verifyUpgradingUserOnboardingFirstScreen(activityTestRule)
            clickCloseButton(activityTestRule)
            verifyHomeScreen()

            relaunchCleanApp(activityTestRule.activityRule)
            clickGetStartedButton(activityTestRule)
            verifyUpgradingUserOnboardingSecondScreen(activityTestRule)
            clickSkipButton(activityTestRule)
            verifyHomeScreen()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1932156
    @Test
    fun upgradingUsersOnboardingSignInButtonTest() {
        homeScreen {
            verifyUpgradingUserOnboardingFirstScreen(activityTestRule)
            clickGetStartedButton(activityTestRule)
            verifyUpgradingUserOnboardingSecondScreen(activityTestRule)
        }.clickUpgradingUserOnboardingSignInButton(activityTestRule) {
            verifyTurnOnSyncMenu()
            mDevice.pressBack()
        }
        homeScreen {
            verifyHomeScreen()
        }
    }
}
