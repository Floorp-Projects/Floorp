/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.helpers.TestHelper;

import java.util.Locale;

import static android.os.Build.VERSION.SDK_INT;
import static android.support.test.espresso.action.ViewActions.click;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.openMenu;
import static org.mozilla.focus.helpers.EspressoHelper.openSettings;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

// This test checks all the headings in the Settings menu are there
@RunWith(AndroidJUnit4.class)
public class SwitchLocaleTest {

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule
            = new ActivityTestRule<MainActivity>(MainActivity.class) {

        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            Context appContext = InstrumentationRegistry.getInstrumentation()
                    .getTargetContext()
                    .getApplicationContext();

            PreferenceManager.getDefaultSharedPreferences(appContext)
                    .edit()
                    .putBoolean(FIRSTRUN_PREF, true)
                    .apply();
        }
    };

    public SwitchLocaleTest() throws UiObjectNotFoundException {
    }

    @BeforeClass
    public static void setupBeforeClass() throws Exception {
        changeLocale("fr");
    }

    @AfterClass
    public static void tearDownAfterClass() throws Exception {
        changeLocale("en");
    }

    @After
    public void tearDown() throws Exception {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @SuppressWarnings("deprecation")
    public static void changeLocale(String locale) {
        Context context = InstrumentationRegistry.getInstrumentation()
                .getTargetContext();

        Resources res = context.getApplicationContext().getResources();
        Configuration config = res.getConfiguration();


        config.setLocale(new Locale(locale));
        if (SDK_INT >= 25) {
            context.createConfigurationContext(config);
        } else {
            res.updateConfiguration(config, res.getDisplayMetrics());
        }
    }

    private UiObject sysDefaultLocale = TestHelper.mDevice.findObject(new UiSelector()
            .className("android.widget.CheckedTextView")
            .instance(0)
            .enabled(true));
    private UiObject localeList = TestHelper.mDevice.findObject(new UiSelector()
            .className("android.widget.ListView")
            .enabled(true));
    private UiObject LanguageSelection = TestHelper.settingsList.getChild(new UiSelector()
            .className("android.widget.LinearLayout")
            .instance(0));
    private UiObject englishHeading = TestHelper.mDevice.findObject(new UiSelector()
            .className("android.widget.TextView")
            .text("Language"));
    private UiObject frenchHeading = TestHelper.mDevice.findObject(new UiSelector()
            .className("android.widget.TextView")
            .text("Paramètres"));

    @Test
    public void EnglishSystemLocaleTest() throws InterruptedException, UiObjectNotFoundException {

        UiObject frenchMenuItem = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("Français"));
        UiObject englishMenuItem = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("System default"));
        UiObject frenchLocaleinEn = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.CheckedTextView")
                .text("Français"));

        /* Go to Settings */
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);

        openSettings();
        LanguageSelection.waitForExists(waitingTime);

        /* system locale is in English, check it is now set to system locale */
        LanguageSelection.click();
        sysDefaultLocale.waitForExists(waitingTime);
        Assert.assertTrue(sysDefaultLocale.isChecked());

        /* change locale to non-english in the setting, verify the locale is changed */
        UiScrollable appViews = new UiScrollable(new UiSelector().scrollable(true));
        appViews.scrollIntoView(frenchLocaleinEn);
        Assert.assertTrue(frenchLocaleinEn.isClickable());
        frenchLocaleinEn.click();

        frenchHeading.waitForExists(waitingTime);
        Assert.assertTrue(frenchHeading.exists());
        Assert.assertTrue(frenchMenuItem.exists());

        /* Exit to main and see the UI is in French as well */
        TestHelper.pressBackKey();
        UiObject frenchTitle = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("Navigation privée automatique.\nNaviguez. Effacez. Recommencez."));
        frenchTitle.waitForExists(waitingTime);
        Assert.assertTrue(frenchTitle.exists());

        openMenu();
        Assert.assertEquals(TestHelper.settingsMenuItem.getText(), "Paramètres");
        Assert.assertEquals(TestHelper.HelpItem.getText(), "Aide");
        TestHelper.settingsMenuItem.click();

        /* re-enter settings, change it back to system locale, verify the locale is changed */
        LanguageSelection.click();
        Assert.assertTrue(frenchLocaleinEn.isChecked());
        appViews.scrollToBeginning(10);
        sysDefaultLocale.waitForExists(waitingTime);
        sysDefaultLocale.click();
        LanguageSelection.waitForExists(waitingTime);
        Assert.assertTrue(englishHeading.exists());
        Assert.assertTrue(englishMenuItem.exists());
        TestHelper.pressBackKey();
        UiObject englishTitle = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("Automatic private browsing.\nBrowse. Erase. Repeat."));
        englishTitle.waitForExists(waitingTime);
        Assert.assertTrue(englishTitle.exists());
        TestHelper.menuButton.perform(click());
        Assert.assertEquals(TestHelper.settingsMenuItem.getText(), "Settings");
        Assert.assertEquals(TestHelper.HelpItem.getText(), "Help");
    }

    @Test
    public void FrenchLocaleTest() throws InterruptedException, UiObjectNotFoundException {

        UiObject frenchMenuItem = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("Valeur par défaut du système"));
        UiObject englishMenuItem = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("English (United States)"));
        UiObject englishLocaleinFr = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.CheckedTextView")
                .text("English (United States)"));

        /* Go to Settings */
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);

        openSettings();
        LanguageSelection.waitForExists(waitingTime);

        /* system locale is in French, check it is now set to system locale */
        frenchHeading.waitForExists(waitingTime);
        Assert.assertTrue(frenchHeading.exists());
        Assert.assertTrue(frenchMenuItem.exists());
        LanguageSelection.click();
        Assert.assertTrue(sysDefaultLocale.isChecked());

        /* change locale to English in the setting, verify the locale is changed */
        UiScrollable appViews = new UiScrollable(new UiSelector().scrollable(true));
        appViews.scrollIntoView(englishLocaleinFr);
        Assert.assertTrue(englishLocaleinFr.isClickable());
        englishLocaleinFr.click();
        englishHeading.waitForExists(waitingTime);
        Assert.assertTrue(englishHeading.exists());
        Assert.assertTrue(englishMenuItem.exists());
    }
}
