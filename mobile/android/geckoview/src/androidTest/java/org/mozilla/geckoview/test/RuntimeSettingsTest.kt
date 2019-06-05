/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.provider.Settings
import android.support.test.InstrumentationRegistry
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import kotlin.math.roundToInt

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
class RuntimeSettingsTest : BaseSessionTest() {

    @Ignore("disable test for frequently failing Bug 1538430")
    @Test fun automaticFontSize() {
        val settings = sessionRule.runtime.settings
        var initialFontSize = 2.15f
        var initialFontInflation = true
        settings.fontSizeFactor = initialFontSize
        assertThat("initial font scale $initialFontSize set",
                settings.fontSizeFactor.toDouble(), closeTo(initialFontSize.toDouble(), 0.05))
        settings.fontInflationEnabled = initialFontInflation
        assertThat("font inflation initially set to $initialFontInflation",
                settings.fontInflationEnabled, `is`(initialFontInflation))


        settings.automaticFontSizeAdjustment = true
        val contentResolver = InstrumentationRegistry.getTargetContext().contentResolver
        val expectedFontSizeFactor = Settings.System.getFloat(contentResolver,
                Settings.System.FONT_SCALE, 1.0f)
        assertThat("Gecko font scale should match system font scale",
                settings.fontSizeFactor.toDouble(), closeTo(expectedFontSizeFactor.toDouble(), 0.05))
        assertThat("font inflation enabled",
                settings.fontInflationEnabled, `is`(true))

        settings.automaticFontSizeAdjustment = false
        assertThat("Gecko font scale restored to previous value",
                settings.fontSizeFactor.toDouble(), closeTo(initialFontSize.toDouble(), 0.05))
        assertThat("font inflation restored to previous value",
                settings.fontInflationEnabled, `is`(initialFontInflation))

        // Now check with that with font inflation initially off, the initial state is still
        // restored correctly after switching auto mode back off.
        // Also reset font size factor back to its default value of 1.0f.
        initialFontSize = 1.0f
        initialFontInflation = false
        settings.fontSizeFactor = initialFontSize
        assertThat("initial font scale $initialFontSize set",
                settings.fontSizeFactor.toDouble(), closeTo(initialFontSize.toDouble(), 0.05))
        settings.fontInflationEnabled = initialFontInflation
        assertThat("font inflation initially set to $initialFontInflation",
                settings.fontInflationEnabled, `is`(initialFontInflation))

        settings.automaticFontSizeAdjustment = true
        assertThat("Gecko font scale should match system font scale",
                settings.fontSizeFactor.toDouble(), closeTo(expectedFontSizeFactor.toDouble(), 0.05))
        assertThat("font inflation enabled",
                settings.fontInflationEnabled, `is`(true))

        settings.automaticFontSizeAdjustment = false
        assertThat("Gecko font scale restored to previous value",
                settings.fontSizeFactor.toDouble(), closeTo(initialFontSize.toDouble(), 0.05))
        assertThat("font inflation restored to previous value",
                settings.fontInflationEnabled, `is`(initialFontInflation))
    }

    @WithDevToolsAPI
    @Test fun fontSize() {
        val settings = sessionRule.runtime.settings
        settings.fontSizeFactor = 1.0f
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        val fontSizeJs = "parseFloat(window.getComputedStyle(document.querySelector('p')).fontSize)"
        val initialFontSize = sessionRule.session.evaluateJS(fontSizeJs) as Double

        val textSizeFactor = 2.0f
        settings.fontSizeFactor = textSizeFactor
        sessionRule.session.reload()
        sessionRule.waitForPageStop()
        var fontSize = sessionRule.session.evaluateJS(fontSizeJs) as Double
        val expectedFontSize = initialFontSize * textSizeFactor
        assertThat("old text size ${initialFontSize}px, new size should be ${expectedFontSize}px",
                fontSize, closeTo(expectedFontSize, 0.1))

        settings.fontSizeFactor = 1.0f
        sessionRule.session.reload()
        sessionRule.waitForPageStop()
        fontSize = sessionRule.session.evaluateJS(fontSizeJs) as Double
        assertThat("text size should be ${initialFontSize}px again",
                fontSize, closeTo(initialFontSize, 0.1))
    }

    @WithDevToolsAPI
    @Test fun fontInflation() {
        val baseFontInflationMinTwips = 120
        val settings = sessionRule.runtime.settings

        settings.fontInflationEnabled = false;
        settings.fontSizeFactor = 1.0f
        val fontInflationPrefJs = "Services.prefs.getIntPref('font.size.inflation.minTwips')"
        var prefValue = (sessionRule.evaluateChromeJS(fontInflationPrefJs) as Double).roundToInt()
        assertThat("Gecko font inflation pref should be turned off",
                prefValue, `is`(0))

        settings.fontInflationEnabled = true;
        prefValue = (sessionRule.evaluateChromeJS(fontInflationPrefJs) as Double).roundToInt()
        assertThat("Gecko font inflation pref should be turned on",
                prefValue, `is`(baseFontInflationMinTwips))

        settings.fontSizeFactor = 2.0f
        prefValue = (sessionRule.evaluateChromeJS(fontInflationPrefJs) as Double).roundToInt()
        assertThat("Gecko font inflation pref should scale with increased font size factor",
                prefValue, greaterThan(baseFontInflationMinTwips))

        settings.fontSizeFactor = 0.5f
        prefValue = (sessionRule.evaluateChromeJS(fontInflationPrefJs) as Double).roundToInt()
        assertThat("Gecko font inflation pref should scale with decreased font size factor",
                prefValue, lessThan(baseFontInflationMinTwips))

        settings.fontSizeFactor = 0.0f
        prefValue = (sessionRule.evaluateChromeJS(fontInflationPrefJs) as Double).roundToInt()
        assertThat("setting font size factor to 0 turns off font inflation",
                prefValue, `is`(0))
        assertThat("GeckoRuntimeSettings returns new font inflation state, too",
                settings.fontInflationEnabled, `is`(false))

        settings.fontSizeFactor = 1.0f
        prefValue = (sessionRule.evaluateChromeJS(fontInflationPrefJs) as Double).roundToInt()
        assertThat("Gecko font inflation pref remains turned off",
                prefValue, `is`(0))
        assertThat("GeckoRuntimeSettings remains turned off",
                settings.fontInflationEnabled, `is`(false))
    }
}
