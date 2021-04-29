/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.setNetworkEnabled

// This tests verify invalid URL and no network connection error pages
@RunWith(AndroidJUnit4ClassRunner::class)
class ErrorPagesTest {
    @get: Rule
    val mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Test
    fun badURLCheckTest() {
        val badURl = "bad.url"

        searchScreen {
        }.loadPage(badURl) {
            verifyPageContent(getStringResource(R.string.mozac_browser_errorpages_unknown_host_title))
            verifyPageContent("Try Again")
        }
    }

    @Ignore("Failing on API 29, working on API 28," +
            "needs investigation: https://github.com/mozilla-mobile/focus-android/issues/4839")
    @Test
    fun noNetworkConnectionErrorPageTest() {
        val pageUrl = "mozilla.org"

        setNetworkEnabled(false)
        searchScreen {
        }.loadPage(pageUrl) {
            verifyPageContent(getStringResource(R.string.mozac_browser_errorpages_unknown_host_title))
            verifyPageContent("Try Again")
            setNetworkEnabled(true)
        }
    }
}
