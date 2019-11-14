/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

private const val SCREEN_HEIGHT = 100
private const val SCREEN_WIDTH = 200

@RunWith(AndroidJUnit4::class)
@MediumTest
class DynamicToolbarTest : BaseSessionTest() {
    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun outOfRangeValue() {
        try {
            sessionRule.display?.run { setDynamicToolbarMaxHeight(SCREEN_HEIGHT + 1) }
            fail("Request should have failed")
        } catch (e: AssertionError) {
            assertThat("Throws an exception when setting values greater than the client height",
                       e.toString(), containsString("maximum height of the dynamic toolbar"))
        }
    }
}
