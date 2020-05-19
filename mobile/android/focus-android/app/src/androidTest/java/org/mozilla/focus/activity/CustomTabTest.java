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
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.web.webdriver.Locator;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;
import org.mozilla.focus.utils.AppConstants;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static androidx.test.espresso.web.sugar.Web.onWebView;
import static androidx.test.espresso.web.webdriver.DriverAtoms.findElement;
import static androidx.test.espresso.web.webdriver.DriverAtoms.getText;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;


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

    @Before
    public void setUp() {

        Context appContext = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();

        // This test runs on both GV and WV.
        // Klar is used to test Geckoview. make sure it's set to Gecko
        TestHelper.selectGeckoForKlar();

    }

    @After
    public void tearDown() {
        activityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void testCustomTabUI() {
        try {
            startWebServer();

            // Launch activity with custom tabs intent
            activityTestRule.launchActivity(createCustomTabIntent());

            // Wait for website to load
            Context appContext = InstrumentationRegistry.getInstrumentation()
                    .getTargetContext()
                    .getApplicationContext();

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
