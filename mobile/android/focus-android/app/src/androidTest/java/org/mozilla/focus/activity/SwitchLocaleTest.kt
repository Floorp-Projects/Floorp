/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.os.Build.VERSION
import androidx.test.espresso.action.ViewActions
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.junit.After
import org.junit.Assert
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TestRule
import org.junit.rules.TestWatcher
import org.junit.runner.Description
import org.junit.runner.RunWith
import org.mozilla.focus.helpers.EspressoHelper.openMenu
import org.mozilla.focus.helpers.EspressoHelper.openSettings
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.waitingTime
import java.util.Locale

// This test checks all the headings in the Settings menu are there
// https://testrail.stage.mozaws.net/index.php?/cases/view/47587
@RunWith(AndroidJUnit4ClassRunner::class)
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

    private val sysDefaultLocale = TestHelper.mDevice.findObject(
        UiSelector()
            .className("android.widget.CheckedTextView")
            .instance(0)
            .enabled(true)
    )
    private val languageHeading = TestHelper.settingsMenu.getChild(
        UiSelector()
            .className("android.widget.LinearLayout")
            .instance(4)
    )
    private val englishHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .className("android.widget.TextView")
            .text("Language")
    )
    private val frenchHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .className("android.widget.TextView")
            .text("Langue")
    )
    private val englishGeneralHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .text("General")
            .resourceId("android:id/title")
    )
    private val frenchGeneralHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .text("Général")
            .resourceId("android:id/title")
    )
    private val englishMozillaHeading = TestHelper.mDevice.findObject(
        UiSelector()
            .text("Mozilla")
            .resourceId("android:id/title")
    )
    private val showHomeScreenTips = TestHelper.mDevice.findObject(
        UiSelector()
            .className("android.widget.Switch")
            .instance(0)
    )

    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun EnglishSystemLocaleTest() {
        val frenchMenuItem = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .text("Français")
        )
        val englishMenuItem = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .text("System default")
        )
        val frenchLocaleinEn = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.CheckedTextView")
                .text("Français")
        )

        /* Go to Settings */TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        openSettings()

        /* Disable home screen tips */englishMozillaHeading.waitForExists(waitingTime)
        englishMozillaHeading.click()
        showHomeScreenTips.waitForExists(waitingTime)
        showHomeScreenTips.click()
        pressBackKey()

        // Open General Settings
        englishGeneralHeading.waitForExists(waitingTime)
        englishGeneralHeading.click()
        languageHeading.waitForExists(waitingTime)

        /* system locale is in English, check it is now set to system locale */languageHeading.click()
        sysDefaultLocale.waitForExists(waitingTime)
        Assert.assertTrue(sysDefaultLocale.isChecked)

        /* change locale to non-english in the setting, verify the locale is changed */
        val appViews = UiScrollable(UiSelector().scrollable(true))
        appViews.scrollIntoView(frenchLocaleinEn)
        Assert.assertTrue(frenchLocaleinEn.isClickable)
        frenchLocaleinEn.click()
        frenchHeading.waitForExists(waitingTime)
        Assert.assertTrue(frenchHeading.exists())
        Assert.assertTrue(frenchMenuItem.exists())

        /* Exit to main and see the UI is in French as well */pressBackKey()
        pressBackKey()
        val frenchTitle = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .text("Navigation privée automatique.\nNaviguez. Effacez. Recommencez.")
        )
        frenchTitle.waitForExists(waitingTime)
        Assert.assertTrue(frenchTitle.exists())
        openMenu()
        Assert.assertEquals(TestHelper.settingsMenuItem.text, "Paramètres")
        Assert.assertEquals(TestHelper.HelpItem.text, "Aide")
        TestHelper.settingsMenuItem.click()

        /* re-enter settings and general settings, change it back to system locale, verify the locale is changed */frenchGeneralHeading.waitForExists(
            waitingTime
        )
        frenchGeneralHeading.click()
        languageHeading.waitForExists(waitingTime)
        languageHeading.click()
        Assert.assertTrue(frenchLocaleinEn.isChecked)
        appViews.scrollToBeginning(10)
        sysDefaultLocale.waitForExists(waitingTime)
        sysDefaultLocale.click()
        languageHeading.waitForExists(waitingTime)
        Assert.assertTrue(englishHeading.exists())
        Assert.assertTrue(englishMenuItem.exists())

        // Exit Settings
        pressBackKey()
        pressBackKey()
        val englishTitle = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .text("Automatic private browsing.\nBrowse. Erase. Repeat.")
        )
        englishTitle.waitForExists(waitingTime)
        Assert.assertTrue(englishTitle.exists())
        openSettings()

        /* Enable home screen tips */englishMozillaHeading.waitForExists(waitingTime)
        englishMozillaHeading.click()
        showHomeScreenTips.waitForExists(waitingTime)
        showHomeScreenTips.click()
        pressBackKey()
        pressBackKey()
        TestHelper.menuButton.perform(ViewActions.click())
        Assert.assertEquals(TestHelper.settingsMenuItem.text, "Settings")
        Assert.assertEquals(TestHelper.HelpItem.text, "Help")
        TestHelper.mDevice.pressBack()
    }

    @Ignore("Failing, see https://github.com/mozilla-mobile/focus-android/issues/4851")
    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun FrenchLocaleTest() {
        val frenchMenuItem = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .text("Valeur par défaut du système")
        )
        val englishMenuItem = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.TextView")
                .text("English (United States)")
        )
        val englishLocaleinFr = TestHelper.mDevice.findObject(
            UiSelector()
                .className("android.widget.CheckedTextView")
                .text("English (United States)")
        )

        /* Go to Settings */TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        openSettings()
        frenchGeneralHeading.waitForExists(waitingTime)
        frenchGeneralHeading.click()
        languageHeading.waitForExists(waitingTime)

        /* system locale is in French, check it is now set to system locale */frenchHeading.waitForExists(
            waitingTime
        )
        Assert.assertTrue(frenchHeading.exists())
        Assert.assertTrue(frenchMenuItem.exists())
        languageHeading.click()
        Assert.assertTrue(sysDefaultLocale.isChecked)

        /* change locale to English in the setting, verify the locale is changed */
        val appViews = UiScrollable(UiSelector().scrollable(true))
        appViews.scrollIntoView(englishLocaleinFr)
        Assert.assertTrue(englishLocaleinFr.isClickable)
        englishLocaleinFr.click()
        englishHeading.waitForExists(waitingTime)
        Assert.assertTrue(englishHeading.exists())
        Assert.assertTrue(englishMenuItem.exists())
        TestHelper.mDevice.pressBack()
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
