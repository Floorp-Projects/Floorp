/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AppAndSystemHelper.runWithCondition
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the new Cookie banner blocker option and functionality.
 */
class CookieBannerBlockerTest : TestSetup() {
    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2419260
    @SmokeTest
    @Test
    fun verifyCookieBannerBlockerSettingsOptionTest() {
        runWithCondition(appContext.settings().shouldUseCookieBannerPrivateMode) {
            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyCookieBannerBlockerButton(enabled = true)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2419273
    @SmokeTest
    @Test
    fun verifyCFRAfterBlockingTheCookieBanner() {
        runWithCondition(appContext.settings().shouldUseCookieBannerPrivateMode) {
            homeScreen {
            }.togglePrivateBrowsingMode()

            navigationToolbar {
            }.enterURLAndEnterToBrowser("voetbal24.be".toUri()) {
                waitForPageToLoad()
                verifyCookieBannerExists(exists = false)
                verifyCookieBannerBlockerCFRExists(exists = true)
            }
        }
    }
}
