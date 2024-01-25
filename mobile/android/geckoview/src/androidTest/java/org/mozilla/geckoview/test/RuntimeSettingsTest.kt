/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.provider.Settings
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.platform.app.InstrumentationRegistry
import junit.framework.TestCase.assertTrue
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Assume.assumeThat
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.BuildConfig
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSession.ProgressDelegate
import org.mozilla.geckoview.WebRequestError
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

@RunWith(AndroidJUnit4::class)
@MediumTest
class RuntimeSettingsTest : BaseSessionTest() {

    @Ignore("disable test for frequently failing Bug 1538430")
    @Test
    fun automaticFontSize() {
        val settings = sessionRule.runtime.settings
        var initialFontSize = 2.15f
        var initialFontInflation = true
        settings.fontSizeFactor = initialFontSize
        assertThat(
            "initial font scale $initialFontSize set",
            settings.fontSizeFactor.toDouble(),
            closeTo(initialFontSize.toDouble(), 0.05),
        )
        settings.fontInflationEnabled = initialFontInflation
        assertThat(
            "font inflation initially set to $initialFontInflation",
            settings.fontInflationEnabled,
            `is`(initialFontInflation),
        )

        settings.automaticFontSizeAdjustment = true
        val contentResolver = InstrumentationRegistry.getInstrumentation().targetContext.contentResolver
        val expectedFontSizeFactor = Settings.System.getFloat(
            contentResolver,
            Settings.System.FONT_SCALE,
            1.0f,
        )
        assertThat(
            "Gecko font scale should match system font scale",
            settings.fontSizeFactor.toDouble(),
            closeTo(expectedFontSizeFactor.toDouble(), 0.05),
        )
        assertThat(
            "font inflation enabled",
            settings.fontInflationEnabled,
            `is`(initialFontInflation),
        )

        settings.automaticFontSizeAdjustment = false
        assertThat(
            "Gecko font scale restored to previous value",
            settings.fontSizeFactor.toDouble(),
            closeTo(initialFontSize.toDouble(), 0.05),
        )
        assertThat(
            "font inflation restored to previous value",
            settings.fontInflationEnabled,
            `is`(initialFontInflation),
        )

        // Now check with that with font inflation initially off, the initial state is still
        // restored correctly after switching auto mode back off.
        // Also reset font size factor back to its default value of 1.0f.
        initialFontSize = 1.0f
        initialFontInflation = false
        settings.fontSizeFactor = initialFontSize
        assertThat(
            "initial font scale $initialFontSize set",
            settings.fontSizeFactor.toDouble(),
            closeTo(initialFontSize.toDouble(), 0.05),
        )
        settings.fontInflationEnabled = initialFontInflation
        assertThat(
            "font inflation initially set to $initialFontInflation",
            settings.fontInflationEnabled,
            `is`(initialFontInflation),
        )

        settings.automaticFontSizeAdjustment = true
        assertThat(
            "Gecko font scale should match system font scale",
            settings.fontSizeFactor.toDouble(),
            closeTo(expectedFontSizeFactor.toDouble(), 0.05),
        )
        assertThat(
            "font inflation enabled",
            settings.fontInflationEnabled,
            `is`(initialFontInflation),
        )

        settings.automaticFontSizeAdjustment = false
        assertThat(
            "Gecko font scale restored to previous value",
            settings.fontSizeFactor.toDouble(),
            closeTo(initialFontSize.toDouble(), 0.05),
        )
        assertThat(
            "font inflation restored to previous value",
            settings.fontInflationEnabled,
            `is`(initialFontInflation),
        )
    }

    @Ignore // Bug 1546297 disabled test on pgo for frequent failures
    @Test
    fun fontSize() {
        val settings = sessionRule.runtime.settings
        settings.fontSizeFactor = 1.0f
        mainSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        val fontSizeJs = "parseFloat(window.getComputedStyle(document.querySelector('p')).fontSize)"
        val initialFontSize = mainSession.evaluateJS(fontSizeJs) as Double

        val textSizeFactor = 2.0f
        settings.fontSizeFactor = textSizeFactor
        mainSession.reload()
        sessionRule.waitForPageStop()
        var fontSize = mainSession.evaluateJS(fontSizeJs) as Double
        val expectedFontSize = initialFontSize * textSizeFactor
        assertThat(
            "old text size ${initialFontSize}px, new size should be ${expectedFontSize}px",
            fontSize,
            closeTo(expectedFontSize, 0.1),
        )

        settings.fontSizeFactor = 1.0f
        mainSession.reload()
        sessionRule.waitForPageStop()
        fontSize = mainSession.evaluateJS(fontSizeJs) as Double
        assertThat(
            "text size should be ${initialFontSize}px again",
            fontSize,
            closeTo(initialFontSize, 0.1),
        )
    }

    @Test fun fontInflation() {
        val baseFontInflationMinTwips = 120
        val settings = sessionRule.runtime.settings

        settings.fontInflationEnabled = false
        settings.fontSizeFactor = 1.0f
        val fontInflationPref = "font.size.inflation.minTwips"

        var prefValue = (sessionRule.getPrefs(fontInflationPref)[0] as Int)
        assertThat(
            "Gecko font inflation pref should be turned off",
            prefValue,
            `is`(0),
        )

        settings.fontInflationEnabled = true
        prefValue = (sessionRule.getPrefs(fontInflationPref)[0] as Int)
        assertThat(
            "Gecko font inflation pref should be turned on",
            prefValue,
            `is`(baseFontInflationMinTwips),
        )

        settings.fontSizeFactor = 2.0f
        prefValue = (sessionRule.getPrefs(fontInflationPref)[0] as Int)
        assertThat(
            "Gecko font inflation pref should scale with increased font size factor",
            prefValue,
            greaterThan(baseFontInflationMinTwips),
        )

        settings.fontSizeFactor = 0.5f
        prefValue = (sessionRule.getPrefs(fontInflationPref)[0] as Int)
        assertThat(
            "Gecko font inflation pref should scale with decreased font size factor",
            prefValue,
            lessThan(baseFontInflationMinTwips),
        )

        settings.fontSizeFactor = 0.0f
        prefValue = (sessionRule.getPrefs(fontInflationPref)[0] as Int)
        assertThat(
            "setting font size factor to 0 turns off font inflation",
            prefValue,
            `is`(0),
        )
        assertThat(
            "GeckoRuntimeSettings returns new font inflation state, too",
            settings.fontInflationEnabled,
            `is`(false),
        )

        settings.fontSizeFactor = 1.0f
        prefValue = (sessionRule.getPrefs(fontInflationPref)[0] as Int)
        assertThat(
            "Gecko font inflation pref remains turned off",
            prefValue,
            `is`(0),
        )
        assertThat(
            "GeckoRuntimeSettings remains turned off",
            settings.fontInflationEnabled,
            `is`(false),
        )
    }

    @Test
    fun largeKeepaliveFactor() {
        val defaultLargeKeepaliveFactor = 10
        val settings = sessionRule.runtime.settings

        val largeKeepaliveFactorPref = "network.http.largeKeepaliveFactor"
        var prefValue = (sessionRule.getPrefs(largeKeepaliveFactorPref)[0] as Int)
        assertThat(
            "default LargeKeepaliveFactor should be 10",
            prefValue,
            `is`(defaultLargeKeepaliveFactor),
        )

        for (factor in 1..10) {
            settings.setLargeKeepaliveFactor(factor)
            prefValue = (sessionRule.getPrefs(largeKeepaliveFactorPref)[0] as Int)
            assertThat(
                "setting LargeKeepaliveFactor to an integer value between 1..10 should work",
                prefValue,
                `is`(factor),
            )
        }

        val sanitizedDefaultLargeKeepaliveFactor = 1

        /**
         * Setting an invalid factor will cause an exception to be throw in debug build.
         * otherwise, the factor will be reset to default when an invalid factor is given.
         */
        try {
            settings.setLargeKeepaliveFactor(128)
            prefValue = (sessionRule.getPrefs(largeKeepaliveFactorPref)[0] as Int)
            assertThat(
                "set LargeKeepaliveFactor to default when input is invalid",
                prefValue,
                `is`(sanitizedDefaultLargeKeepaliveFactor),
            )
        } catch (e: Exception) {
            if (BuildConfig.DEBUG_BUILD) {
                assertTrue("Should have an exception in DEBUG_BUILD", true)
            }
        }
    }

    @Test
    fun aboutConfig() {
        // This is broken in automation because document channel is enabled by default
        assumeThat(sessionRule.env.isAutomation, equalTo(false))
        val settings = sessionRule.runtime.settings

        assertThat(
            "about:config should be disabled by default",
            settings.aboutConfigEnabled,
            equalTo(false),
        )

        mainSession.loadUri("about:config")
        mainSession.waitUntilCalled(object : NavigationDelegate {
            @AssertCalled
            override fun onLoadError(session: GeckoSession, uri: String?, error: WebRequestError):
                GeckoResult<String>? {
                assertThat("about:config should not load.", uri, equalTo("about:config"))
                return null
            }
        })

        settings.aboutConfigEnabled = true

        mainSession.delegateDuringNextWait(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("about:config load should succeed", success, equalTo(true))
            }
        })

        mainSession.loadUri("about:config")
        mainSession.waitForPageStop()
    }

    @Test
    fun globalPrivacyControlEnabling() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val geckoRuntimeSettings = sessionRule.runtime.settings

        geckoRuntimeSettings.setGlobalPrivacyControl(true)

        val gpcValue = mainSession.evaluateJS(
            "window.navigator.globalPrivacyControl",
        )

        assertThat(
            "Global Privacy Control should now be enabled",
            gpcValue,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control runtime settings should now be enabled in normal tabs",
            geckoRuntimeSettings.globalPrivacyControl,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control runtime settings should still be enabled in private tabs",
            geckoRuntimeSettings.globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        val globalPrivacyControl =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.enabled").get(0)) as Boolean
        val globalPrivacyControlPrivateMode =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.pbmode.enabled").get(0)) as Boolean
        val globalPrivacyControlFunctionality = (
            sessionRule.getPrefs("privacy.globalprivacycontrol.functionality.enabled").get(0)
            ) as Boolean

        assertThat(
            "Global Privacy Control should be enabled in normal tabs",
            globalPrivacyControl,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control should still be in private tabs",
            globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control Functionality flag should be enabled",
            globalPrivacyControlFunctionality,
            equalTo(true),
        )
    }

    @Test
    fun globalPrivacyControlDisabling() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val geckoRuntimeSettings = sessionRule.runtime.settings

        geckoRuntimeSettings.setGlobalPrivacyControl(false)

        val gpcValue = mainSession.evaluateJS(
            "window.navigator.globalPrivacyControl",
        )

        assertThat(
            "Global Privacy Control should now be disabled in normal mode",
            gpcValue,
            equalTo(false),
        )

        assertThat(
            "Global Privacy Control runtime settings should now be enabled in normal tabs",
            geckoRuntimeSettings.globalPrivacyControl,
            equalTo(false),
        )

        assertThat(
            "Global Privacy Control runtime settings should still be enabled in private tabs",
            geckoRuntimeSettings.globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        val globalPrivacyControl =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.enabled").get(0)) as Boolean
        val globalPrivacyControlPrivateMode =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.pbmode.enabled").get(0)) as Boolean
        val globalPrivacyControlFunctionality = (
            sessionRule.getPrefs("privacy.globalprivacycontrol.functionality.enabled").get(0)
            ) as Boolean

        assertThat(
            "Global Privacy Control should be enabled in normal tabs",
            globalPrivacyControl,
            equalTo(false),
        )

        assertThat(
            "Global Privacy Control should still be enabled in private tabs",
            globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control Functionality flag should still be enabled",
            globalPrivacyControlFunctionality,
            equalTo(true),
        )
    }
}
