/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import androidx.annotation.IdRes;
import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.IdlingRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.SessionLoadedIdlingResource;
import org.mozilla.focus.utils.AppConstants;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static org.hamcrest.Matchers.containsString;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.openMenu;

// This test visits each page and checks whether some essential elements are being displayed
// https://testrail.stage.mozaws.net/index.php?/cases/view/81664
@RunWith(AndroidJUnit4.class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
public class AccessAboutAndYourRightsPagesTest {

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new ActivityTestRule<MainActivity>(MainActivity.class) {

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

    private SessionLoadedIdlingResource loadingIdlingResource;

    @Before
    public void setUp() {

        loadingIdlingResource = new SessionLoadedIdlingResource();
        IdlingRegistry.getInstance().register(loadingIdlingResource);
    }

    @After
    public void tearDown() {
        IdlingRegistry.getInstance().unregister(loadingIdlingResource);

        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void AboutAndYourRightsPagesTest() {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // What's new page
        openMenu();
        clickMenuItem(R.id.whats_new);
        assertWebsiteUrlContains("support.mozilla.org");
        pressBack();

        // Help page
        openMenu();
        clickMenuItem(R.id.help);
        assertWebsiteUrlContains("what-firefox-focus-android");
        pressBack();

        // Go to settings "About" page
        openMenu();
        clickMenuItem(R.id.settings);
        final String mozillaMenuLabel = context.getString(R.string.preference_category_mozilla, context.getString(R.string.app_name));
        onView(withText(mozillaMenuLabel))
                .check(matches(isDisplayed()))
                .perform(click());

        final String aboutLabel = context.getString(R.string.preference_about, context.getString(R.string.app_name));
        onView(withText(aboutLabel))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withId(R.id.infofragment))
                .check(matches(isDisplayed()))
                .perform(click());
        pressBack();  // This takes to Mozilla Submenu of settings

        // "Your rights" page
        onView(withText(context.getString(R.string.your_rights)))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withId(R.id.infofragment))
                .check(matches(isDisplayed()))
                .perform(click());
    }

    private void clickMenuItem(@IdRes int id) {
        onView(withId(id))
                .check(matches(isDisplayed()))
                .perform(click());
    }

    private void assertWebsiteUrlContains(String substring) {
        onView(withId(R.id.display_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(containsString(substring))));
    }
}
