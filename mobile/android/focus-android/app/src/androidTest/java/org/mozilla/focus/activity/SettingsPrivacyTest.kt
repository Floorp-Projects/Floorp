/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests the Privacy and Security settings menus and options
@RunWith(AndroidJUnit4ClassRunner::class)
class SettingsPrivacyTest {
    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @SmokeTest
    @Test
    fun verifyCookiesAndSiteDataItemsTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            verifyCookiesAndSiteDataSection()
            clickBlockCookies()
            verifyBlockCookiesPrompt()
            clickCancelBlockCookiesPrompt()
            clickSitePermissionsButton()
            verifySitePermissionsSection()
            clickAutoPlayOption()
            verifyAutoplaySection()
        }
    }
}
