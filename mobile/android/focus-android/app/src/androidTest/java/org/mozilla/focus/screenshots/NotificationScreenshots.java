/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.UiObject;
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
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.containsString;

/**
 * This test has been super flaky in the past so we moved it to its own test. This way we wat least
 * only lose screenshots of this test in case it fails.
 */
@RunWith(AndroidJUnit4.class)
public class NotificationScreenshots extends ScreenshotTest {

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    private MockWebServer webServer;

    @Before
    public void setUpWebServer() throws IOException {
        webServer = new MockWebServer();

        // Test page
        webServer.enqueue(new MockResponse().setBody(TestHelper.readTestAsset("plain_test.html")));
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
    public void takeScreenshotOfNotification() throws Exception {
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText(webServer.url("/").toString()), pressImeActionButton());

        onView(withId(R.id.display_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(containsString(webServer.getHostName()))));

        final UiObject openAction = device.findObject(new UiSelector()
                .descriptionContains(getString(R.string.notification_action_open))
                .resourceId("android:id/action0")
                .enabled(true));

        device.openNotification();

        try {
            if (!openAction.waitForExists(waitingTime)) {
                // The notification is not expanded. Let's expand it now.
                device.findObject(new UiSelector()
                        .text(getString(R.string.app_name)))
                        .swipeDown(20);

                assertTrue(openAction.waitForExists(waitingTime));
            }

            Screengrab.screenshot("DeleteHistory_NotificationBar");
        } finally {
            // Close notification tray again
            device.pressBack();
        }
    }
}
