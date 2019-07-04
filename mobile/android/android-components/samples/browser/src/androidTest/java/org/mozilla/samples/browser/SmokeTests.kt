/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.os.SystemClock
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.pressImeActionButton
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.samples.browser.rules.WebserverRule
import java.util.concurrent.TimeUnit

private const val INITIAL_WAIT_SECONDS = 5L
private const val WAIT_FOR_WEB_CONTENT_SECONDS = 15L

/**
 * A collection of "smoke tests" to verify that the basic browsing functionality is working.
 */
@RunWith(AndroidJUnit4::class)
@LargeTest
class SmokeTests {
    @get:Rule
    val activityRule: ActivityTestRule<BrowserActivity> = ActivityTestRule(BrowserActivity::class.java)

    @get:Rule
    val webserverRule: WebserverRule = WebserverRule()

    /**
     * This test loads a website from a local webserver by typing into the URL bar. After that it verifies that the
     * web content is visible.
     */
    @Test
    fun loadWebsiteTest() {
        // Meh! We need a better idle strategy here. Because of bug 1441059 our load request gets lost if it happens
        // to fast and then only "about:blank" gets loaded. So a "quick" fix here is to just wait a bit.
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1441059
        SystemClock.sleep(TimeUnit.SECONDS.toMillis(INITIAL_WAIT_SECONDS))

        onView(withId(mozilla.components.browser.toolbar.R.id.mozac_browser_toolbar_url_view))
            .perform(click())

        onView(withId(mozilla.components.browser.toolbar.R.id.mozac_browser_toolbar_edit_url_view))
            .perform(replaceText(webserverRule.url()), pressImeActionButton())

        verifyWebsiteContent("Hello World!")

        onView(withId(mozilla.components.browser.toolbar.R.id.mozac_browser_toolbar_url_view))
            .check(matches(withText(webserverRule.url())))
    }
}

private fun verifyWebsiteContent(text: String) {
    val device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    device.waitForIdle()

    val waitingTime: Long = TimeUnit.SECONDS.toMillis(WAIT_FOR_WEB_CONTENT_SECONDS)

    assertTrue(device
        .findObject(UiSelector()
        .textContains(text))
        .waitForExists(waitingTime))
}
