/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.preference.PreferenceManager;
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
import org.mozilla.focus.activity.helpers.SessionLoadedIdlingResource;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.pressImeActionButton;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasFocus;
import static android.support.test.espresso.matcher.ViewMatchers.isChecked;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static org.hamcrest.Matchers.not;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test toggles blocking within the browser view
// mozilla.org site has one google analytics tracker - tests will see whether this gets blocked properly
@RunWith(AndroidJUnit4.class)
public class ToggleBlockTest {
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
        mActivityTestRule.getActivity().finishAndRemoveTask();

        IdlingRegistry.getInstance().unregister(loadingIdlingResource);
    }

    @Test
    public void SimpleToggleTest() throws UiObjectNotFoundException {
        // Load mozilla.org
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(typeText("mozilla"), pressImeActionButton());

        // The blocking badge is not disabled
        onView(withId(R.id.block))
                .check(matches(not(isDisplayed())));

        // Open the menu
        onView(withId(R.id.menuView))
                .perform(click());

        // Check that the tracker count is 1.
        onView(withId(R.id.trackers_count))
                .check(matches(withText("1")));

        // Disable blocking
        onView(withId(R.id.blocking_switch))
                .check(matches(isChecked()))
                .perform(click());

        // Now the blocking badge is visible
        onView(withId(R.id.block))
                .check(matches(isDisplayed()));

        // Open the menu again. Now the switch is disabled and the tracker count should be disabled.
        onView(withId(R.id.menuView))
                .perform(click());
        onView(withId(R.id.blocking_switch))
                .check(matches(not(isChecked())));
        onView(withId(R.id.trackers_count))
                .check(matches(withText("-")));
    }
}