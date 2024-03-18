/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Context
import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for Reader View UI.
 */
class ReaderViewRobot {

    fun verifyAppearanceFontGroup(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceFontGroup: Trying to verify that the font group buttons are visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_font_group),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceFontGroup: Verified that the font group buttons are visible: $visible")
    }

    fun verifyAppearanceFontSansSerif(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceFontSansSerif: Trying to verify that the sans serif font button is visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_font_sans_serif),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceFontSansSerif: Verified that the sans serif font button is visible: $visible")
    }

    fun verifyAppearanceFontSerif(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceFontSerif: Trying to verify that the serif font button is visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_font_serif),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceFontSerif: Verified that the serif font button is visible: $visible")
    }

    fun verifyAppearanceFontDecrease(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceFontDecrease: Trying to verify that the decrease font button is visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_font_size_decrease),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceFontDecrease: Verified that the decrease font button is visible: $visible")
    }

    fun verifyAppearanceFontIncrease(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceFontIncrease: Trying to verify that the increase font button is visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_font_size_increase),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceFontIncrease: Verified that the increase font button is visible: $visible")
    }

    fun verifyAppearanceColorGroup(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceColorGroup: Trying to verify that the color group buttons are visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_color_scheme_group),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceColorGroup: Verified that the color group buttons are visible: $visible")
    }

    fun verifyAppearanceColorSepia(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceColorSepia: Trying to verify that the sepia color button is visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_color_sepia),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceColorSepia: Verified that the sepia color button is visible: $visible")
    }

    fun verifyAppearanceColorDark(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceColorDark: Trying to verify that the dark color button is visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_color_dark),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceColorDark: Verified that the dark color button is visible: $visible")
    }

    fun verifyAppearanceColorLight(visible: Boolean = false) {
        Log.i(TAG, "verifyAppearanceColorLight: Trying to verify that the light color button is visible: $visible")
        onView(
            withId(R.id.mozac_feature_readerview_color_light),
        ).check(
            matches(withEffectiveVisibility(visibleOrGone(visible))),
        )
        Log.i(TAG, "verifyAppearanceColorLight: Verified that the light color button is visible: $visible")
    }

    fun verifyAppearanceFontIsActive(fontType: String) {
        Log.i(TAG, "verifyAppearanceFontIsActive: Trying to verify that the font type is: $fontType")
        val fontTypeKey: String = "mozac-readerview-fonttype"

        val prefs = InstrumentationRegistry.getInstrumentation()
            .targetContext.getSharedPreferences(
                "mozac_feature_reader_view",
                Context.MODE_PRIVATE,
            )

        assertEquals(fontType, prefs.getString(fontTypeKey, ""))
        Log.i(TAG, "verifyAppearanceFontIsActive: Verified that the font type is: $fontType")
    }

    fun verifyAppearanceFontSize(expectedFontSize: Int) {
        Log.i(TAG, "verifyAppearanceFontSize: Trying to verify that the font size is: $expectedFontSize")
        val fontSizeKey: String = "mozac-readerview-fontsize"

        val prefs = InstrumentationRegistry.getInstrumentation()
            .targetContext.getSharedPreferences(
                "mozac_feature_reader_view",
                Context.MODE_PRIVATE,
            )

        val fontSizeKeyValue = prefs.getInt(fontSizeKey, 3)

        assertEquals(expectedFontSize, fontSizeKeyValue)
        Log.i(TAG, "verifyAppearanceFontSize: Verified that the font size is: $expectedFontSize")
    }

    fun verifyAppearanceColorSchemeChange(expectedColorScheme: String) {
        Log.i(TAG, "verifyAppearanceColorSchemeChange: Trying to verify that the color scheme is: $expectedColorScheme")
        val colorSchemeKey: String = "mozac-readerview-colorscheme"

        val prefs = InstrumentationRegistry.getInstrumentation()
            .targetContext.getSharedPreferences(
                "mozac_feature_reader_view",
                Context.MODE_PRIVATE,
            )

        assertEquals(expectedColorScheme, prefs.getString(colorSchemeKey, ""))
        Log.i(TAG, "verifyAppearanceColorSchemeChange: Verified that the color scheme is: $expectedColorScheme")
    }

    class Transition {

        fun closeAppearanceMenu(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "closeAppearanceMenu: Trying to click device back button")
            mDevice.pressBack()
            Log.i(TAG, "closeAppearanceMenu: Clicked device back button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun toggleSansSerif(interact: ReaderViewRobot.() -> Unit): Transition {
            fun sansSerifButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_sans_serif),
                )
            Log.i(TAG, "toggleSansSerif: Trying to click sans serif button")
            sansSerifButton().click()
            Log.i(TAG, "toggleSansSerif: Clicked sans serif button")

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleSerif(interact: ReaderViewRobot.() -> Unit): Transition {
            fun serifButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_serif),
                )
            Log.i(TAG, "toggleSerif: Trying to click serif button")
            serifButton().click()
            Log.i(TAG, "toggleSerif: Clicked serif button")

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleFontSizeDecrease(interact: ReaderViewRobot.() -> Unit): Transition {
            fun fontSizeDecrease() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_size_decrease),
                )
            Log.i(TAG, "toggleFontSizeDecrease: Trying to click the decrease font button")
            fontSizeDecrease().click()
            Log.i(TAG, "toggleFontSizeDecrease: Clicked the decrease font button")

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleFontSizeIncrease(interact: ReaderViewRobot.() -> Unit): Transition {
            fun fontSizeIncrease() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_size_increase),
                )
            Log.i(TAG, "toggleFontSizeIncrease: Trying to click the increase font button")
            fontSizeIncrease().click()
            Log.i(TAG, "toggleFontSizeIncrease: Clicked the increase font button")

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleColorSchemeChangeLight(interact: ReaderViewRobot.() -> Unit): Transition {
            fun toggleLightColorSchemeButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_color_light),
                )
            Log.i(TAG, "toggleColorSchemeChangeLight: Trying to click the light color button")
            toggleLightColorSchemeButton().click()
            Log.i(TAG, "toggleColorSchemeChangeLight: Clicked the light color button")

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleColorSchemeChangeDark(interact: ReaderViewRobot.() -> Unit): Transition {
            fun toggleDarkColorSchemeButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_color_dark),
                )
            Log.i(TAG, "toggleColorSchemeChangeDark: Trying to click the dark color button")
            toggleDarkColorSchemeButton().click()
            Log.i(TAG, "toggleColorSchemeChangeDark: Clicked the dark color button")

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleColorSchemeChangeSepia(interact: ReaderViewRobot.() -> Unit): Transition {
            fun toggleSepiaColorSchemeButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_color_sepia),
                )
            Log.i(TAG, "toggleColorSchemeChangeSepia: Trying to click the sepia color button")
            toggleSepiaColorSchemeButton().click()
            Log.i(TAG, "toggleColorSchemeChangeSepia: Clicked the sepia color button")

            ReaderViewRobot().interact()
            return Transition()
        }
    }
}

private fun visibleOrGone(visibility: Boolean) =
    if (visibility) ViewMatchers.Visibility.VISIBLE else ViewMatchers.Visibility.GONE
