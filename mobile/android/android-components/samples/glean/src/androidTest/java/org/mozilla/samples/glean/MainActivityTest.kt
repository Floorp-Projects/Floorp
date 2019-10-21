/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.rule.ActivityTestRule
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

    @Test
    fun checkGleanClickData() {
        // We don't reset the storage in this test as the GleanTestRule does not
        // work nicely in instrumented test. Just check the current value, increment
        // by one and make it the expected value.
        val expectedValue = if (GleanTestMetrics.counter.testHasValue()) {
            GleanTestMetrics.counter.testGetValue() + 1
        } else {
            1
        }

        // Simulate a click on the button.
        onView(withId(R.id.buttonGenerateData)).perform(click())

        // Use the Glean testing API to check if the expected data was recorded.
        assertTrue(GleanTestMetrics.counter.testHasValue())
        assertEquals(expectedValue, GleanTestMetrics.counter.testGetValue())
    }
}
