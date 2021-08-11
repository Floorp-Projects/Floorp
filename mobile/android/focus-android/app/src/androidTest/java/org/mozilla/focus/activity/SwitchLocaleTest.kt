/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.os.Build.VERSION
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TestRule
import org.junit.rules.TestWatcher
import org.junit.runner.Description
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.StringsHelper.EN_LANGUAGE_MENU_HEADING
import org.mozilla.focus.helpers.StringsHelper.FR_ENGLISH_LOCALE
import org.mozilla.focus.helpers.StringsHelper.FR_GENERAL_HEADING
import org.mozilla.focus.helpers.StringsHelper.FR_HELP
import org.mozilla.focus.helpers.StringsHelper.FR_LANGUAGE_MENU
import org.mozilla.focus.helpers.StringsHelper.EN_FRENCH_LOCALE
import org.mozilla.focus.helpers.StringsHelper.FR_MOTTO
import org.mozilla.focus.helpers.StringsHelper.FR_SETTINGS
import org.mozilla.focus.helpers.StringsHelper.FR_LANGUAGE_SYSTEM_DEFAULT
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.verifyTranslatedTextExists
import java.util.Locale

// Testing some translated elements when switching locales and using system default
class SwitchLocaleTest {
    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @get: Rule
    var watcher: TestRule = object : TestWatcher() {
        override fun starting(description: Description) {
            println("Starting test: " + description.methodName)
            if (description.methodName == "FrenchLocaleTest") {
                changeLocale("fr")
            }
        }
    }

    @After
    fun tearDown() {
        changeLocale("en")
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Test
    fun englishSystemLocaleTest() {
        disableHomeScreenTips()

        /* Go to Settings and change language to French*/
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openGeneralSettingsMenu {
            openLanguageSelectionMenu()
            verifyLanguageSelected("System default")
            selectLanguage(EN_FRENCH_LOCALE)
            verifyTranslatedTextExists(FR_LANGUAGE_MENU)
            exitToTop()
        }
        /* Exit to main and see the UI is in French as well */
        homeScreen {
            verifyTranslatedTextExists(FR_MOTTO)
        }.openMainMenu {
            verifyTranslatedTextExists(FR_SETTINGS)
            verifyTranslatedTextExists(FR_HELP)
        /* change back to system locale, verify the locale is changed */
        }.openSettings(FR_SETTINGS) {
        }.openGeneralSettingsMenu(FR_GENERAL_HEADING) {
            openLanguageSelectionMenu(FR_LANGUAGE_MENU)
            selectLanguage(FR_LANGUAGE_SYSTEM_DEFAULT)
            verifyTranslatedTextExists(EN_LANGUAGE_MENU_HEADING)
            exitToTop()
        }
        homeScreen {
        }.openMainMenu {
            verifySettingsButtonExists()
            verifyHelpPageLinkExists()
        }
    }

    @Ignore("Failing, see https://github.com/mozilla-mobile/focus-android/issues/4851")
    @Test
    fun frenchLocaleTest() {
        /* Go to Settings */
        homeScreen {
        }.openMainMenu {
        }.openSettings(FR_SETTINGS) {
        }.openGeneralSettingsMenu(FR_GENERAL_HEADING) {
            openLanguageSelectionMenu(FR_LANGUAGE_MENU)
            verifyLanguageSelected(FR_LANGUAGE_SYSTEM_DEFAULT)
            /* change locale to English, verify the locale is changed */
            selectLanguage(FR_ENGLISH_LOCALE)
            verifyTranslatedTextExists(EN_LANGUAGE_MENU_HEADING)
            exitToTop()
        }
        homeScreen {
        }.openMainMenu {
            verifySettingsButtonExists()
            verifyHelpPageLinkExists()
        }
    }

    companion object {
        @Suppress("Deprecation")
        fun changeLocale(locale: String?) {
            val context = InstrumentationRegistry.getInstrumentation()
                .targetContext
            val res = context.applicationContext.resources
            val config = res.configuration
            config.setLocale(Locale(locale!!))
            if (VERSION.SDK_INT >= 25) {
                context.createConfigurationContext(config)
            } else {
                res.updateConfiguration(config, res.displayMetrics)
            }
        }
    }
}

private fun disableHomeScreenTips() {
    homeScreen {
    }.openMainMenu {
    }.openSettings {
    }.openMozillaSettingsMenu {
        switchHomeScreenTips()
        exitToTop()
    }
}
