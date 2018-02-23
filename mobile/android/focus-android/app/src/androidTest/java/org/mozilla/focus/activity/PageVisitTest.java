/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.preference.PreferenceManager;
import android.support.annotation.IdRes;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.IdlingRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObjectNotFoundException;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.SessionLoadedIdlingResource;

import static android.support.test.espresso.Espresso.onData;
import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.Espresso.pressBack;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.PreferenceMatchers.withTitleText;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static org.hamcrest.Matchers.containsString;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.assertToolbarMatchesText;
import static org.mozilla.focus.helpers.EspressoHelper.openMenu;

// This test visits each page and checks whether some essential elements are being displayed
@RunWith(AndroidJUnit4.class)
public class PageVisitTest {

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
    public void tearDown() throws Exception {
        IdlingRegistry.getInstance().unregister(loadingIdlingResource);

        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void visitPagesTest() throws InterruptedException, UiObjectNotFoundException {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // What's new page

        openMenu();

        clickMenuItem(R.id.whats_new);
        assertWebsiteUrlContains("support.mozilla.org");

        pressBack();

        // Help page

        openMenu();

        clickMenuItem(R.id.help);
        assertToolbarMatchesText(R.string.menu_help);

        pressBack();

        // Go to settings

        openMenu();
        clickMenuItem(R.id.settings);

        // "About" page

        final String aboutLabel = context.getString(R.string.preference_about, context.getString(R.string.app_name));

        onData(withTitleText(aboutLabel))
                .check(matches(isDisplayed()))
                .perform(click());

        assertToolbarMatchesText(R.string.menu_about);

        pressBack();

        // "Your rights" page

        onData(withTitleText(context.getString(R.string.your_rights)))
                .check(matches(isDisplayed()))
                .perform(click());

        assertToolbarMatchesText(R.string.your_rights);
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
