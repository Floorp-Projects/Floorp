/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

class TestReviewSettings(
    override var numberOfAppLaunches: Int = 0,
    var isDefault: Boolean = false,
    override var lastReviewPromptTimeInMillis: Long = 0
) : ReviewSettings {
    override val isDefaultBrowser: Boolean
        get() = isDefault
}

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class ReviewPromptControllerTest {
    @Test
    fun promptReviewDoesNotSetMillis() = runBlockingTest {
        var promptWasCalled = false
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = false,
            lastReviewPromptTimeInMillis = 0L
        )

        val controller = ReviewPromptController(
            testContext,
            settings,
            { 100L },
            { promptWasCalled = true }
        )

        controller.promptReview(HomeActivity())
        assertEquals(settings.lastReviewPromptTimeInMillis, 0L)
        assertFalse(promptWasCalled)
    }

    @Test
    fun promptReviewSetsMillisIfSuccessful() = runBlockingTest {
        var promptWasCalled = false
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L
        )

        val controller = ReviewPromptController(
            testContext,
            settings,
            { 100L },
            { promptWasCalled = true }
        )

        controller.promptReview(HomeActivity())
        assertEquals(100L, settings.lastReviewPromptTimeInMillis)
        assertTrue(promptWasCalled)
    }

    @Test
    fun trackApplicationLaunch() {
        val settings = TestReviewSettings(
            numberOfAppLaunches = 4,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L
        )

        val controller = ReviewPromptController(
            testContext,
            settings,
            { 100L }
        )

        assertEquals(4, settings.numberOfAppLaunches)
        controller.trackApplicationLaunch()
        assertEquals(5, settings.numberOfAppLaunches)
    }

    @Test
    fun shouldShowPrompt() {
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L
        )

        val controller = ReviewPromptController(
            testContext,
            settings,
            { 1598416882805L }
        )

        // Test first success criteria
        assertTrue(controller.shouldShowPrompt())

        // Test with last prompt approx 4 months earlier
        settings.apply {
            numberOfAppLaunches = 5
            isDefault = true
            lastReviewPromptTimeInMillis = 1588048882804L
        }

        assertTrue(controller.shouldShowPrompt())

        // Test without being the default browser
        settings.apply {
            numberOfAppLaunches = 5
            isDefault = false
            lastReviewPromptTimeInMillis = 1595824882805L
        }

        assertFalse(controller.shouldShowPrompt())

        // Test with number of app launches < 5
        settings.apply {
            numberOfAppLaunches = 4
            isDefault = true
            lastReviewPromptTimeInMillis = 0L
        }

        assertFalse(controller.shouldShowPrompt())

        // Test with last prompt less than 4 months ago
        settings.apply {
            numberOfAppLaunches = 5
            isDefault = true
            lastReviewPromptTimeInMillis = 1595824882905L
        }

        assertFalse(controller.shouldShowPrompt())
    }
}
