/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;
import org.mozilla.focus.utils.AppConstants;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.pressImeActionButton;
import static android.support.test.espresso.action.ViewActions.replaceText;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasFocus;
import static android.support.test.espresso.matcher.ViewMatchers.isClickable;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static org.hamcrest.Matchers.containsString;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime;
import static org.mozilla.focus.web.WebViewProviderKt.ENGINE_PREF_STRING_KEY;

// This test checks whether URL and displayed site are in sync
@RunWith(AndroidJUnit4.class)
public class URLMismatchTest {

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new ActivityTestRule<MainActivity>(MainActivity.class) {
        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            final Context appContext = InstrumentationRegistry.getInstrumentation()
                    .getTargetContext()
                    .getApplicationContext();

            PreferenceManager.getDefaultSharedPreferences(appContext)
                    .edit()
                    .putBoolean(FIRSTRUN_PREF, true)
                    .apply();

            // This test runs on both GV and WV.
            // Klar is used to test Geckoview. make sure it's set to Gecko
            if (AppConstants.INSTANCE.isKlarBuild() && !AppConstants.INSTANCE.isGeckoBuild()) {
                PreferenceManager.getDefaultSharedPreferences(appContext)
                        .edit()
                        .putBoolean(ENGINE_PREF_STRING_KEY, true)
                        .apply();
            }
        }
    };

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void MismatchTest() {
        // Type "mozilla" into the URL bar.
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText("mozilla"));

        // Verify that the search hint is displayed and click it.
        onView(withId(R.id.searchView))
                .check(matches(isDisplayed()))
                .check(matches(withText("Search for mozilla")))
                .check(matches(isClickable()))
                .perform(click());

        // A WebView is displayed
        onView(withId(R.id.webview))
                .check(matches(isDisplayed()));

        // The displayed URL contains mozilla. Click on it to edit it again.
        onView(withId(R.id.display_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(containsString("mozilla"))))
                .perform(click());

        // Type "moz" - Verify that it auto-completes to "mozilla.org" and then load the website
        onView(withId(R.id.urlView))
                .perform(click(), replaceText("mozilla"))
                .check(matches(withText("mozilla.org")))
                .perform(pressImeActionButton());

        // Wait until site is loaded
        onView(withId(R.id.webview))
                .check(matches(isDisplayed()));
        TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime);

        // The displayed URL contains www.mozilla.org
        onView(withId(R.id.display_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(containsString("www.mozilla.org"))));
    }
}
