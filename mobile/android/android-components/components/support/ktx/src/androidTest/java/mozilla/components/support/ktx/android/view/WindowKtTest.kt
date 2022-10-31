/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.graphics.Color
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.filters.SdkSuppress
import mozilla.components.support.ktx.TestActivity
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class WindowKtTest {
    @get:Rule
    internal val activityRule: ActivityScenarioRule<TestActivity> = ActivityScenarioRule(TestActivity::class.java)

    @SdkSuppress(minSdkVersion = 23)
    @Test
    fun whenALightColorIsAppliedToStatusBarThenSetIsAppearanceLightStatusBarsToTrue() {
        activityRule.scenario.onActivity {
            it.window.setStatusBarTheme(Color.WHITE)

            assertTrue(it.window.getWindowInsetsController().isAppearanceLightStatusBars)
        }
    }

    @Test
    fun whenADarkColorIsAppliedToStatusBarThenSetIsAppearanceLightStatusBarsToFalse() {
        activityRule.scenario.onActivity {
            it.window.setStatusBarTheme(Color.BLACK)

            assertFalse(it.window.getWindowInsetsController().isAppearanceLightStatusBars)
        }
    }

    @SdkSuppress(minSdkVersion = 23)
    @Test
    fun whenALightColorIsAppliedToNavigationBarThemeThenSetIsAppearanceLightNavigationBarsToTrue() {
        activityRule.scenario.onActivity {
            it.window.setNavigationBarTheme(Color.WHITE)

            assertTrue(it.window.getWindowInsetsController().isAppearanceLightNavigationBars)
        }
    }

    @Test
    fun whenADarkColorIsAppliedToNavigationBarThemeThenSetIsAppearanceLightNavigationBarsToFalse() {
        activityRule.scenario.onActivity {
            it.window.setNavigationBarTheme(Color.BLACK)

            assertFalse(it.window.getWindowInsetsController().isAppearanceLightNavigationBars)
        }
    }
}
