/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the presence of home screen elements
 *
 */

class HomeScreenTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    @get:Rule
    val activityTestRule = HomeActivityTestRule()
    @Test
    fun homeScreenItemsTest() {
        homeScreen {
            verifyHomeScreen()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()
            verifyHomeToolbar()
            verifyHomeComponent()
        }
    }
}
