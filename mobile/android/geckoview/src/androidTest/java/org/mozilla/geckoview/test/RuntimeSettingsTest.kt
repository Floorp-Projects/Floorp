/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import kotlin.math.roundToInt

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
class RuntimeSettingsTest : BaseSessionTest() {

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
