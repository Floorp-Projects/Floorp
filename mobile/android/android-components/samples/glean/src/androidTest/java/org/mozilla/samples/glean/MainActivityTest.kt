/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.rule.ActivityTestRule
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.mozilla.samples.glean.GleanMetrics.Test as GleanTestMetrics

import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class MainActivityTest {
    @get:Rule
    val activityRule: ActivityTestRule<MainActivity> = ActivityTestRule(MainActivity::class.java)

    @get:Rule
    val gleanRule = GleanTestRule(
        ApplicationProvider.getApplicationContext(),
        Configuration(serverEndpoint = getPingServerAddress())
    )

    @Test
    fun checkGleanClickData() {
        // Simulate a click on the button.
        onView(withId(R.id.buttonGenerateData)).perform(click())

        // Use the Glean testing API to check if the expected data was recorded.
        assertTrue(GleanTestMetrics.testCounter.testHasValue())
        assertEquals(1, GleanTestMetrics.testCounter.testGetValue())
    }

    @Test
    fun checkGleanClickDataAgain() {
        // Repeat the same test as above: this will fail if the GleanTestRule fails to
        // clear the Glean SDK state.
        checkGleanClickData()
    }
}