package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper.runWithLauncherIntent
import org.mozilla.fenix.ui.robots.homeScreen

class OnboardingTest {

    @get:Rule
    val activityTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(launchActivity = false),
        ) { it.activity }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2122321
    @Test
    fun verifyFirstOnboardingCardItemsTest() {
        runWithLauncherIntent(activityTestRule) {
            homeScreen {
                verifyFirstOnboardingCard(activityTestRule)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2122334
    @SmokeTest
    @Test
    fun verifyFirstOnboardingCardItemsFunctionalityTest() {
        runWithLauncherIntent(activityTestRule) {
            homeScreen {
                clickNotNowOnboardingButton(activityTestRule)
                verifySecondOnboardingCard(activityTestRule)
                swipeSecondOnboardingCardToRight()
            }.clickSetAsDefaultBrowserOnboardingButton(activityTestRule) {
                verifyAndroidDefaultAppsMenuAppears()
            }.goBackToOnboardingScreen {
                verifySecondOnboardingCard(activityTestRule)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2122343
    @Test
    fun verifySecondOnboardingCardItemsTest() {
        runWithLauncherIntent(activityTestRule) {
            homeScreen {
                clickNotNowOnboardingButton(activityTestRule)
                verifySecondOnboardingCard(activityTestRule)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2122344
    @SmokeTest
    @Test
    fun verifySecondOnboardingCardSignInFunctionalityTest() {
        runWithLauncherIntent(activityTestRule) {
            homeScreen {
                clickNotNowOnboardingButton(activityTestRule)
                verifySecondOnboardingCard(activityTestRule)
            }.clickSignInOnboardingButton(activityTestRule) {
                verifyTurnOnSyncMenu()
            }
        }
    }
}
