/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.equalTo
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting

@RunWith(AndroidJUnit4::class)
@MediumTest
class RuntimeSettingsDefaultsTest : BaseSessionTest() {
    @Test
    fun globalPrivacyControlDefaultsInNormalMode() {
        mainSession.loadTestPath(HELLO2_HTML_PATH)
        mainSession.waitForPageStop()

        val geckoRuntimeSettings = sessionRule.runtime.settings
        val globalPrivacyControl =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.enabled").get(0)) as Boolean
        val globalPrivacyControlPrivateMode =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.pbmode.enabled").get(0)) as Boolean
        val globalPrivacyControlFunctionality = (
            sessionRule.getPrefs("privacy.globalprivacycontrol.functionality.enabled").get(0)
            ) as Boolean

        assertThat(
            "Global Privacy Control runtime settings should be disabled by default in normal tabs",
            geckoRuntimeSettings.globalPrivacyControl,
            equalTo(false),
        )

        assertThat(
            "Global Privacy Control runtime settings should be enabled by default in private tabs",
            geckoRuntimeSettings.globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control should be disabled by default in normal tabs",
            globalPrivacyControl,
            equalTo(false),
        )

        assertThat(
            "Global Privacy Control should be disabled by default in private tabs",
            globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control Functionality enabled by default",
            globalPrivacyControlFunctionality,
            equalTo(true),
        )

        val gpcValue = mainSession.evaluateJS(
            "window.navigator.globalPrivacyControl",
        )

        assertThat(
            "Global Privacy Control should be disabled in normal mode",
            gpcValue,
            equalTo(false),
        )
    }

    @Test
    @Setting.List(Setting(key = Setting.Key.USE_PRIVATE_MODE, value = "true"))
    fun globalPrivacyControlDefaultsInPrivateMode() {
        mainSession.loadTestPath(HELLO2_HTML_PATH)
        mainSession.waitForPageStop()

        val geckoRuntimeSettings = sessionRule.runtime.settings
        val globalPrivacyControl =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.enabled").get(0)) as Boolean
        val globalPrivacyControlPrivateMode =
            (sessionRule.getPrefs("privacy.globalprivacycontrol.pbmode.enabled").get(0)) as Boolean
        val globalPrivacyControlFunctionality =
            (
                sessionRule.getPrefs("privacy.globalprivacycontrol.functionality.enabled")
                    .get(0)
                ) as Boolean

        assertThat(
            "Global Privacy Control runtime settings should be disabled by default in normal tabs",
            geckoRuntimeSettings.globalPrivacyControl,
            equalTo(false),
        )

        assertThat(
            "Global Privacy Control runtime settings should be enabled by default in private tabs",
            geckoRuntimeSettings.globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control should be disabled by default in normal tabs",
            globalPrivacyControl,
            equalTo(false),
        )

        assertThat(
            "Global Privacy Control should be disabled by default in private tabs",
            globalPrivacyControlPrivateMode,
            equalTo(true),
        )

        assertThat(
            "Global Privacy Control Functionality enabled by default",
            globalPrivacyControlFunctionality,
            equalTo(true),
        )

        val gpcValue = mainSession.evaluateJS(
            "window.navigator.globalPrivacyControl",
        )

        assertThat(
            "Global Privacy Control should be disabled in private mode",
            gpcValue,
            equalTo(true),
        )
    }
}
