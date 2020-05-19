/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;

import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiScrollable;
import androidx.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasFocus;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.AllOf.allOf;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.openSettings;
import static org.mozilla.focus.helpers.TestHelper.mDevice;

// https://testrail.stage.mozaws.net/index.php?/cases/view/104577
@RunWith(AndroidJUnit4.class)
public class URLExceptionTest {
    private String site = "680news.com";

    private UiObject exceptionsTitle = TestHelper.mDevice.findObject(new UiSelector()
            .text("Exceptions")
            .resourceId("android:id/title"));

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

            // This test runs on both GV and WV.
            // Klar is used to test Geckoview. make sure it's set to Gecko
            TestHelper.selectGeckoForKlar();
        }
    };

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    private void OpenExceptionsFragment() throws UiObjectNotFoundException {
        mDevice.waitForIdle();
        openSettings();

        onView(withText("Privacy & Security"))
                .check(matches(isDisplayed()))
                .perform(click());

        /* change locale to non-english in the setting, verify the locale is changed */
        UiScrollable appViews = new UiScrollable(new UiSelector().scrollable(true));
        appViews.scrollIntoView(exceptionsTitle);
        if (exceptionsTitle.isEnabled()) {
            exceptionsTitle.click();
        }

        mDevice.waitForIdle();
    }

    @Test
    public void AddAndRemoveExceptionTest() throws UiObjectNotFoundException {
        addException(site);

        OpenExceptionsFragment();

        onView(allOf(withText(site), withId(R.id.domainView)))
                .check(matches(isDisplayed()));
        mDevice.waitForIdle();

        removeException();
        mDevice.waitForIdle();

        // Should be in main menu
        onView(withText("Privacy & Security"))
                .check(matches(isDisplayed()));

        /* change locale to non-english in the setting, verify the locale is changed */
        UiScrollable appViews = new UiScrollable(new UiSelector().scrollable(true));
        appViews.scrollIntoView(exceptionsTitle);

        onView(withText("Exceptions"))
                .check(matches(isDisplayed()))
                .check(matches(not(isEnabled())));
    }

    private void addException(String url) {
        // Load site
        TestHelper.inlineAutocompleteEditText.waitForExists(TestHelper.waitingTime);
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(typeText(url), pressImeActionButton());

        TestHelper.waitForWebContent();
        TestHelper.progressBar.waitUntilGone(TestHelper.waitingTime);

        // The blocking badge is not disabled
        onView(withId(R.id.block))
                .check(matches(not(isDisplayed())));

        // Open the menu
        onView(withId(R.id.menuView))
                .perform(click());
        onView(withId(R.id.trackers_count))
                .check(matches(not(withText("-"))));

        // Disable blocking
        onView(withId(R.id.blocking_switch))
                .check(matches(isChecked()));

        onView(withId(R.id.blocking_switch))
                .perform(click());
        TestHelper.waitForWebContent();
    }

    private void removeException() {
        mDevice.waitForIdle();
        openActionBarOverflowOrOptionsMenu(InstrumentationRegistry.getContext());
        mDevice.waitForIdle();   // wait until dialog fully appears
        onView(withText("Remove"))
                .perform(click());
        onView(withId(R.id.checkbox))
                .perform((click()));
        onView(withId(R.id.remove))
                .perform((click()));
    }

    @Test
    public void removeAllExceptionsTest() throws UiObjectNotFoundException {
        addException(site);

        OpenExceptionsFragment();

        mDevice.waitForIdle();
        onView(withId(R.id.removeAllExceptions))
                .perform(click());

        mDevice.waitForIdle();

        // Should be in main menu
        onView(withText("Privacy & Security"))
                .check(matches(isDisplayed()));

        /* change locale to non-english in the setting, verify the locale is changed */
        UiScrollable appViews = new UiScrollable(new UiSelector().scrollable(true));
        appViews.scrollIntoView(exceptionsTitle);

        onView(withText("Exceptions"))
                .check(matches(isDisplayed()))
                .check(matches(not(isEnabled())));
    }
}
