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
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.SystemClock;
import android.preference.PreferenceManager;
import android.support.customtabs.CustomTabsIntent;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiSelector;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.utils.AppConstants;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.action.ViewActions.click;
import static org.mozilla.focus.activity.TestHelper.mDevice;
import static org.mozilla.focus.activity.TestHelper.waitingTime;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

@RunWith(AndroidJUnit4.class)
public class CustomTabTest {
    private static final String MENU_ITEM_LABEL = "TestItem4223";
    private static final String ACTION_BUTTON_DESCRIPTION = "TestButton1337";

    private MockWebServer webServer;

    private UiObject titleMsg = TestHelper.mDevice.findObject(new UiSelector()
            .description("focus test page")
            .enabled(true));

    private UiObject actionButton = TestHelper.mDevice.findObject(new UiSelector()
            .description(ACTION_BUTTON_DESCRIPTION)
            .enabled(true));

    private UiObject shareMenuitem = TestHelper.mDevice.findObject(new UiSelector()
            .text("Share…")
            .enabled(true));

    private UiObject customMenuItem = TestHelper.mDevice.findObject(new UiSelector()
            .text(MENU_ITEM_LABEL)
            .enabled(true));

    private UiObject closeButton = TestHelper.mDevice.findObject(new UiSelector()
            .description("Return to previous app")
            .enabled(true));

    @Rule
    public ActivityTestRule<MainActivity> activityTestRule  = new ActivityTestRule<>(
            MainActivity.class, true, false);

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
            Assert.assertTrue(titleMsg.waitForExists(waitingTime));

            // Verify action button is visible
            Assert.assertTrue(actionButton.waitForExists(waitingTime));

            // Open the menu
            TestHelper.menuButton.perform(click());

            // Verify share action is visible
            Assert.assertTrue(shareMenuitem.waitForExists(waitingTime));

            // Verify custom menu item is visible
            Assert.assertTrue(customMenuItem.waitForExists(waitingTime));

            // Close menu again
            mDevice.pressBack();

            // Verify close button is visible - Click it to close custom tab.
            Assert.assertTrue(closeButton.waitForExists(waitingTime));
            closeButton.click();
        } finally {
            stopWebServer();
        }
    }

    private Intent createCustomTabIntent() {
        final Context appContext = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();

        final PendingIntent pendingIntent = PendingIntent.getActivity(appContext, 0, new Intent(), 0);

        final Drawable drawable = appContext.getResources().getDrawable(R.drawable.ic_delete, null);
        Assert.assertNotNull(drawable);

        final CustomTabsIntent customTabsIntent = new CustomTabsIntent.Builder()
                .addMenuItem(MENU_ITEM_LABEL, pendingIntent)
                .addDefaultShareMenuItem()
                .setActionButton(toBitmap(drawable), ACTION_BUTTON_DESCRIPTION, pendingIntent, true)
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

    private Bitmap toBitmap(Drawable drawable) {
        Bitmap bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }
}
