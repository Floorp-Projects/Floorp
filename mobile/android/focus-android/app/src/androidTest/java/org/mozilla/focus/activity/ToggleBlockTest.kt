/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.Espresso
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.hamcrest.Matchers
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import org.mozilla.focus.helpers.TestHelper.waitForWebContent
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

// This test toggles blocking within the browser view
// mozilla.org site has one google analytics tracker - tests will see whether this gets blocked properly
@Ignore("Failing because of: https://github.com/mozilla-mobile/focus-android/issues/4729")
@RunWith(AndroidJUnit4ClassRunner::class)
class ToggleBlockTest {
    private var webServer: MockWebServer? = null
    private var loadingIdlingResource: SessionLoadedIdlingResource? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        loadingIdlingResource = SessionLoadedIdlingResource()
        IdlingRegistry.getInstance().register(loadingIdlingResource)

        webServer = MockWebServer()
        try {
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("ad.html"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("ad.html"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("ad.html"))
            )
            webServer!!.start()
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }
    }

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
        IdlingRegistry.getInstance().unregister(loadingIdlingResource)
        try {
            webServer!!.close()
            webServer!!.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
    }

    @Suppress("LongMethod")
    @SmokeTest
    @Test
    fun SimpleToggleTest() {
        // Load mozilla.org
        TestHelper.inlineAutocompleteEditText.waitForExists(TestHelper.waitingTime)
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_edit_url_view))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.hasFocus()))
            .perform(
                ViewActions.typeText(webServer!!.url(TEST_PATH).toString()),
                ViewActions.pressImeActionButton()
            )
        waitForWebContent()
        TestHelper.progressBar.waitUntilGone(TestHelper.waitingTime)

        // Blocking is on
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_tracking_protection_indicator))
            .check(
                ViewAssertions.matches(
                    ViewMatchers.withContentDescription(
                        R.string.mozac_browser_toolbar_content_description_tracking_protection_on_no_trackers_blocked
                    )
                )
            )

        // Open the menu
        Espresso.onView(ViewMatchers.withId(R.id.menuView))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.trackers_count))
            .check(ViewAssertions.matches(Matchers.not(ViewMatchers.withText("-"))))

        // Disable blocking
        Espresso.onView(ViewMatchers.withId(R.id.blocking_switch))
            .check(ViewAssertions.matches(ViewMatchers.isChecked()))
        Espresso.onView(ViewMatchers.withId(R.id.blocking_switch))
            .perform(ViewActions.click())
        waitForWebContent()

        // Now the blocking badge is visible
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_tracking_protection_indicator))
            .check(
                ViewAssertions.matches(
                    ViewMatchers.withContentDescription(
                        R.string.mozac_browser_toolbar_content_description_tracking_protection_off_for_a_site1
                    )
                )
            )
        Espresso.onView(ViewMatchers.withId(R.id.webview)).perform(ViewActions.click())

        // Open the menu again. Now the switch is disabled and the tracker count should be disabled.
        Espresso.onView(ViewMatchers.withId(R.id.menuView))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.blocking_switch))
            .check(ViewAssertions.matches(Matchers.not(ViewMatchers.isChecked())))
        Espresso.onView(ViewMatchers.withId(R.id.trackers_count))
            .check(ViewAssertions.matches(ViewMatchers.withText("-")))

        // Close the menu
        pressBackKey()

        // Erase
        TestHelper.floatingEraseButton.perform(ViewActions.click())
        TestHelper.erasedMsg.waitForExists(waitingTime)

        // Load the same site again, it should now be in the exceptions list
        try {
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("ad.html"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("ad.html"))
            )
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("ad.html"))
            )
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }
        TestHelper.inlineAutocompleteEditText.waitForExists(TestHelper.waitingTime)
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_edit_url_view))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.hasFocus()))
            .perform(
                ViewActions.typeText(webServer!!.url(TEST_PATH).toString()),
                ViewActions.pressImeActionButton()
            )
        waitForWebContent()
        TestHelper.progressBar.waitUntilGone(TestHelper.waitingTime)

        // Blocking is on
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_tracking_protection_indicator))
            .check(
                ViewAssertions.matches(
                    ViewMatchers.withContentDescription(
                        R.string.mozac_browser_toolbar_content_description_tracking_protection_on_no_trackers_blocked
                    )
                )
            )

        // Open the menu again. Now the switch is disabled and the tracker count should be disabled.
        Espresso.onView(ViewMatchers.withId(R.id.menuView))
            .perform(ViewActions.click())
        Espresso.onView(ViewMatchers.withId(R.id.blocking_switch))
            .check(ViewAssertions.matches(Matchers.not(ViewMatchers.isChecked())))
        Espresso.onView(ViewMatchers.withId(R.id.trackers_count))
            .check(ViewAssertions.matches(ViewMatchers.withText("-")))
    }

    companion object {
        private const val TEST_PATH = "/"
    }
}
