/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Context
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for Reader View UI.
 */
class ReaderViewRobot {

    fun verifyAppearanceFontGroup(visible: Boolean = false): ViewInteraction =
        assertAppearanceFontGroup(visible)

    fun verifyAppearanceFontSansSerif(visible: Boolean = false): ViewInteraction =
        assertAppearanceFontSansSerif(visible)

    fun verifyAppearanceFontSerif(visible: Boolean = false): ViewInteraction =
        assertAppearanceFontSerif(visible)

    fun verifyAppearanceFontDecrease(visible: Boolean = false): ViewInteraction =
        assertAppearanceFontDecrease(visible)

    fun verifyAppearanceFontIncrease(visible: Boolean = false): ViewInteraction =
        assertAppearanceFontIncrease(visible)

    fun verifyAppearanceColorGroup(visible: Boolean = false): ViewInteraction =
        assertAppearanceColorGroup(visible)

    fun verifyAppearanceColorSepia(visible: Boolean = false): ViewInteraction =
        assertAppearanceColorSepia(visible)

    fun verifyAppearanceColorDark(visible: Boolean = false): ViewInteraction =
        assertAppearanceColorDark(visible)

    fun verifyAppearanceColorLight(visible: Boolean = false): ViewInteraction =
        assertAppearanceColorLight(visible)

    fun verifyAppearanceFontIsActive(fontType: String) {
        val fontTypeKey: String = "mozac-readerview-fonttype"

        val prefs = InstrumentationRegistry.getInstrumentation()
            .targetContext.getSharedPreferences(
                "mozac_feature_reader_view",
                Context.MODE_PRIVATE,
            )

        assertEquals(fontType, prefs.getString(fontTypeKey, ""))
    }

    fun verifyAppearanceFontSize(expectedFontSize: Int) {
        val fontSizeKey: String = "mozac-readerview-fontsize"

        val prefs = InstrumentationRegistry.getInstrumentation()
            .targetContext.getSharedPreferences(
                "mozac_feature_reader_view",
                Context.MODE_PRIVATE,
            )

        val fontSizeKeyValue = prefs.getInt(fontSizeKey, 3)

        assertEquals(expectedFontSize, fontSizeKeyValue)
    }

    fun verifyAppearanceColorSchemeChange(expectedColorScheme: String) {
        val colorSchemeKey: String = "mozac-readerview-colorscheme"

        val prefs = InstrumentationRegistry.getInstrumentation()
            .targetContext.getSharedPreferences(
                "mozac_feature_reader_view",
                Context.MODE_PRIVATE,
            )

        assertEquals(expectedColorScheme, prefs.getString(colorSchemeKey, ""))
    }

    class Transition {

        fun closeAppearanceMenu(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressBack()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun toggleSansSerif(interact: ReaderViewRobot.() -> Unit): Transition {
            fun sansSerifButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_sans_serif),
                )

            sansSerifButton().click()

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleSerif(interact: ReaderViewRobot.() -> Unit): Transition {
            fun serifButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_serif),
                )

            serifButton().click()

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleFontSizeDecrease(interact: ReaderViewRobot.() -> Unit): Transition {
            fun fontSizeDecrease() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_size_decrease),
                )

            fontSizeDecrease().click()

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleFontSizeIncrease(interact: ReaderViewRobot.() -> Unit): Transition {
            fun fontSizeIncrease() =
                onView(
                    withId(R.id.mozac_feature_readerview_font_size_increase),
                )

            fontSizeIncrease().click()

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleColorSchemeChangeLight(interact: ReaderViewRobot.() -> Unit): Transition {
            fun toggleLightColorSchemeButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_color_light),
                )

            toggleLightColorSchemeButton().click()

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleColorSchemeChangeDark(interact: ReaderViewRobot.() -> Unit): Transition {
            fun toggleDarkColorSchemeButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_color_dark),
                )

            toggleDarkColorSchemeButton().click()

            ReaderViewRobot().interact()
            return Transition()
        }

        fun toggleColorSchemeChangeSepia(interact: ReaderViewRobot.() -> Unit): Transition {
            fun toggleSepiaColorSchemeButton() =
                onView(
                    withId(R.id.mozac_feature_readerview_color_sepia),
                )

            toggleSepiaColorSchemeButton().click()

            ReaderViewRobot().interact()
            return Transition()
        }
    }
}

fun readerViewRobot(interact: ReaderViewRobot.() -> Unit): ReaderViewRobot.Transition {
    ReaderViewRobot().interact()
    return ReaderViewRobot.Transition()
}

private fun assertAppearanceFontGroup(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_font_group),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceFontSansSerif(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_font_sans_serif),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceFontSerif(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_font_serif),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceFontDecrease(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_font_size_decrease),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceFontIncrease(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_font_size_increase),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceColorDark(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_color_dark),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceColorLight(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_color_light),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceColorSepia(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_color_sepia),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun assertAppearanceColorGroup(visible: Boolean) =
    onView(
        withId(R.id.mozac_feature_readerview_color_scheme_group),
    ).check(
        matches(withEffectiveVisibility(visibleOrGone(visible))),
    )

private fun visibleOrGone(visibility: Boolean) =
    if (visibility) ViewMatchers.Visibility.VISIBLE else ViewMatchers.Visibility.GONE
