/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.net.Uri;
import android.preference.PreferenceManager;
import android.support.customtabs.CustomTabsIntent;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.web.webdriver.Locator;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static android.support.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static android.support.test.espresso.web.sugar.Web.onWebView;
import static android.support.test.espresso.web.webdriver.DriverAtoms.findElement;
import static android.support.test.espresso.web.webdriver.DriverAtoms.getText;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

@RunWith(AndroidJUnit4.class)
public class CustomTabTest {
    private static final String MENU_ITEM_LABEL = "TestItem4223";
    private static final String ACTION_BUTTON_DESCRIPTION = "TestButton1337";
    private static final String TEST_PAGE_HEADER_ID = "header";
    private static final String TEST_PAGE_HEADER_TEXT = "focus test page";

    private MockWebServer webServer;

    @Rule
    public ActivityTestRule<IntentReceiverActivity> activityTestRule  = new ActivityTestRule<>(
            IntentReceiverActivity.class, true, false);

    @After
    public void tearDown() throws Exception {
        activityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void testCustomTabUI() throws Exception {
        try {
            startWebServer();

            // Launch activity with custom tabs intent
            activityTestRule.launchActivity(createCustomTabIntent());

            // Wait for website to load
            onWebView()
                    .withElement(findElement(Locator.ID, TEST_PAGE_HEADER_ID))
                    .check(webMatches(getText(), equalTo(TEST_PAGE_HEADER_TEXT)));

            // Verify action button is visible
            onView(withContentDescription(ACTION_BUTTON_DESCRIPTION))
                    .check(matches(isDisplayed()));

            // Open menu
            onView(withId(R.id.menuView))
                    .perform(click());

            // Verify share action is visible
            onView(withId(R.id.share))
                    .check(matches(isDisplayed()));

            // Verify custom menu item is visible
            onView(withText(MENU_ITEM_LABEL))
                    .check(matches(isDisplayed()));

            // Close the menu again
            Espresso.pressBack();

            // Verify close button is visible - Click it to close custom tab.
            onView(withId(R.id.customtab_close))
                    .check(matches(isDisplayed()))
                    .perform(click());
        } finally {
            stopWebServer();
            // Open recent apps list, then close focus from there
            TestHelper.pressRecentAppsKey();
            UiObject dismissFocusBtn = TestHelper.mDevice.findObject(new UiSelector()
                    .resourceId("com.android.systemui:id/dismiss_task")
                    .descriptionContains("Dismiss Firefox Focus")
                    .enabled(true));
            dismissFocusBtn.click();
            dismissFocusBtn.waitUntilGone(waitingTime);
            assertTrue(!dismissFocusBtn.exists());
            TestHelper.pressHomeKey();
        }
    }

    private Intent createCustomTabIntent() {
        final Context appContext = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();

        final PendingIntent pendingIntent = PendingIntent.getActivity(appContext, 0, new Intent(), 0);

        final CustomTabsIntent customTabsIntent = new CustomTabsIntent.Builder()
                .addMenuItem(MENU_ITEM_LABEL, pendingIntent)
                .addDefaultShareMenuItem()
                .setActionButton(createTestBitmap(), ACTION_BUTTON_DESCRIPTION, pendingIntent, true)
                .setToolbarColor(Color.MAGENTA)
                .build();

        customTabsIntent.intent.setData(Uri.parse(webServer.url("/").toString()));

        return customTabsIntent.intent;
    }

    private void startWebServer() {
        final Context appContext = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();

        PreferenceManager.getDefaultSharedPreferences(appContext)
                .edit()
                .putBoolean(FIRSTRUN_PREF, true)
                .apply();

        webServer = new MockWebServer();

        try {
            webServer.enqueue(new MockResponse()
                    .setBody(TestHelper.readTestAsset("plain_test.html")));
            webServer.start();
        } catch (IOException e) {
            throw new AssertionError("Could not start web server", e);
        }
    }

    private void stopWebServer() {
        try {
            webServer.close();
            webServer.shutdown();
        } catch (IOException e) {
            throw new AssertionError("Could not stop web server", e);
        }
    }

    private Bitmap createTestBitmap() {
        final Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.GREEN);
        return bitmap;
    }
}
