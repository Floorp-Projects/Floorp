/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.os.SystemClock;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiScrollable;
import androidx.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import tools.fastlane.screengrab.Screengrab;
import tools.fastlane.screengrab.locale.LocaleTestRule;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasFocus;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

@RunWith(AndroidJUnit4.class)
public class AllowListScreenshots extends ScreenshotTest {

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    private MockWebServer webServer;

    @Before
    public void setUpWebServer() throws IOException {
        webServer = new MockWebServer();

        // Test page
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("image_test.html")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("rabbit.jpg")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("download.jpg")));
        // Download
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("image_test.html")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("rabbit.jpg")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("download.jpg")));
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
    public void takeScreenshotsOfMenuandAllowlist() throws UiObjectNotFoundException {
        SystemClock.sleep(5000);
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText(webServer.url("/").toString()));

        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(pressImeActionButton());

        device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/webview")
                .enabled(true))
                .waitForExists(waitingTime);

        TestHelper.menuButton.perform(click());
        Screengrab.screenshot("BrowserViewMenu");
        onView(withId(R.id.blocking_switch)).perform(click());

        // Open setting
        onView(withId(R.id.menuView))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withId(R.id.settings))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withText(R.string.preference_privacy_and_security_header)).perform(click());

        UiScrollable settingsView = new UiScrollable(new UiSelector().scrollable(true));
        if (settingsView.exists()) {        // On tablet, this will not be found
            settingsView.scrollToEnd(5);
            onView(withText(R.string.preference_exceptions)).perform(click());
        }

        onView(withId(R.id.removeAllExceptions))
                .check(matches(isDisplayed()));
        Screengrab.screenshot("ExceptionsDialog");
        onView(withId(R.id.removeAllExceptions))
                .perform(click());

    }
}
