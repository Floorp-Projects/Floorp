/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule

// This test checks all the headings in the Settings menu are there
@RunWith(AndroidJUnit4ClassRunner::class)
class SettingsTest {
    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Test
    fun accessSettingsMenuTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
            verifySettingsMenuItems()
        }
    }

    @Test
    fun verifyGeneralSettingsMenuTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openGeneralSettingsMenu {
            verifyGeneralSettingsItems()
        }
    }

    @Test
    fun verifyPrivacySettingsMenuTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openPrivacySettingsMenu {
            verifyPrivacySettingsItems()
        }
    }

    @Test
    fun verifySearchSettingsMenuTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openSearchSettingsMenu {
            verifySearchSettingsItems()
        }
    }

    @Test
    fun verifyAdvancedSettingsMenuTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openAdvancedSettingsMenu {
            verifyAdvancedSettingsItems()
        }
    }

    @Test
    fun verifyMozillaMenuTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openMozillaSettingsMenu {
            verifyMozillaMenuItems()
        }
    }
}
