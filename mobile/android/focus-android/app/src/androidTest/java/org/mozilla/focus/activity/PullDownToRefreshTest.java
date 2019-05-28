/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.web.webdriver.Locator;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import android.view.View;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.action.ViewActions.swipeDown;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasChildCount;
import static androidx.test.espresso.matcher.ViewMatchers.hasFocus;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayingAtLeast;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static androidx.test.espresso.web.sugar.Web.onWebView;
import static androidx.test.espresso.web.webdriver.DriverAtoms.findElement;
import static androidx.test.espresso.web.webdriver.DriverAtoms.getText;
import static org.hamcrest.Matchers.containsString;


// https://testrail.stage.mozaws.net/index.php?/cases/view/94146
@RunWith(AndroidJUnit4.class)
@Ignore("Pull to refresh is currently disabled in all builds")
public class PullDownToRefreshTest {
    private static final String COUNTER = "counter";
    private static final String FIRST_TIME = "1";
    private static final String SECOND_TIME = "2";

    private MockWebServer webServer;
    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new MainActivityFirstrunTestRule(false);

    @Before
    public void setUpWebServer() throws IOException {
        webServer = new MockWebServer();

        // Test page
        webServer.enqueue(new MockResponse().setBody(TestHelper.readTestAsset("counter.html")));
        webServer.enqueue(new MockResponse().setBody(TestHelper.readTestAsset("counter.html")));
    }

    @After
    public void tearDownWebServer() {
        try {
            webServer.close();
            webServer.shutdown();
        } catch (IOException e) {
            throw new AssertionError("Could not stop web server", e);
        }
    }

    @Test
    public void pullDownToRefreshTest() {

       onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText(webServer.url("/").toString()), pressImeActionButton());

        onView(withId(R.id.display_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(containsString(webServer.getHostName()))));

        onWebView()
                .withElement(findElement(Locator.ID, COUNTER))
                .check(webMatches(getText(), containsString(FIRST_TIME)));

        onView(withId(R.id.swipe_refresh))
                .check(matches(isDisplayed()));

        // Swipe down to refresh, spinner is shown (2nd child) and progress bar is shown
        onView(withId(R.id.swipe_refresh))
              .perform(withCustomConstraints(swipeDown(), isDisplayingAtLeast(85)))
              .check(matches(hasChildCount(2)));

        onWebView()
                .withElement(findElement(Locator.ID, COUNTER))
                .check(webMatches(getText(), containsString(SECOND_TIME)));
    }

    public static ViewAction withCustomConstraints(final ViewAction action, final Matcher<View> constraints) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return constraints;
            }

            @Override
            public String getDescription() {
                return action.getDescription();
            }

            @Override
            public void perform(UiController uiController, View view) {
                action.perform(uiController, view);
            }
        };
    }
}
