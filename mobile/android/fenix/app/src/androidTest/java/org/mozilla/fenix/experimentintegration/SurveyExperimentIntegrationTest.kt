/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experimentintegration

import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying functionality of the message survey surface
 */
class SurveyExperimentIntegrationTest {
    private val surveyURL = "qsurvey.mozilla.com"
    private val experimentName = "Viewpoint"

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        TestHelper.appContext.settings().showSecretDebugMenuThisSession = true
    }

    @After
    fun tearDown() {
        TestHelper.appContext.settings().showSecretDebugMenuThisSession = false
    }

    @Test
    fun checkSurveyNavigatesCorrectly() {
        browserScreen {
            verifySurveyButton()
        }.clickSurveyButton {}

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openExperimentsMenu {
            verifyExperimentExists(experimentName)
        }
    }
}
