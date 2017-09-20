/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.action.ViewActions.click;
import static org.mozilla.focus.activity.TestHelper.waitingTime;
import static org.mozilla.focus.activity.TestHelper.webPageLoadwaitingTime;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

@RunWith(AndroidJUnit4.class)
public class AddtoHSTest {
    private static final String TEST_PATH = "/";

    private Context appContext;
    private MockWebServer webServer;

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule  = new ActivityTestRule<MainActivity>(MainActivity.class) {
        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            appContext = InstrumentationRegistry.getInstrumentation()
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
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("plain_test.html")));

                webServer.start();
            } catch (IOException e) {
                throw new AssertionError("Could not start web server", e);
            }
        }

        @Override
        protected void afterActivityFinished() {
            super.afterActivityFinished();

            try {
                webServer.close();
                webServer.shutdown();
            } catch (IOException e) {
                throw new AssertionError("Could not stop web server", e);
            }
        }
    };

    @After
    public void tearDown() throws Exception {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    private UiObject titleMsg = TestHelper.mDevice.findObject(new UiSelector()
            .description("focus test page")
            .enabled(true));
    UiObject welcomeBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("com.android.launcher3:id/cling_dismiss_longpress_info")
            .text("GOT IT")
            .enabled(true));

    void removeWelcomeOverlay() throws UiObjectNotFoundException {
        final String FOCUS_DEBUG_APP = "org.mozilla.focus.debug";

        // Note: in case of some older simulators, the Welcome overlay covers
        // where the shortcut icons should be!
        TestHelper.pressHomeKey();
        if (welcomeBtn.exists()) {
            welcomeBtn.click();
        }
        // Re-Launch the app
        Context context = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();
        final Intent intent = context.getPackageManager()
                .getLaunchIntentForPackage(FOCUS_DEBUG_APP);
        context.startActivity(intent);
    }

    private void handleShortcutLayoutDialog() throws UiObjectNotFoundException {
        TestHelper.AddautoBtn.waitForExists(waitingTime);
        TestHelper.AddautoBtn.click();
        TestHelper.AddautoBtn.waitUntilGone(waitingTime);
        TestHelper.pressHomeKey();
    }

    @Test
    public void AddToHomeScreenTest() throws InterruptedException, UiObjectNotFoundException, IOException {

        UiObject shortcutIcon = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .description("For Testing Purpose")
                .enabled(true));

        removeWelcomeOverlay();

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.progressBar.waitForExists(webPageLoadwaitingTime);
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime));
        Assert.assertTrue("Website title loaded", titleMsg.exists());

        TestHelper.menuButton.perform(click());
        // If the menu item is not clickable, wait and retry
        while (!TestHelper.AddtoHSmenuItem.waitForExists(waitingTime)) {
            TestHelper.pressBackKey();
            Thread.sleep(waitingTime);
            TestHelper.menuButton.perform(click());
        }
        TestHelper.AddtoHSmenuItem.click();

        // Add to Home screen dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime);

        Assert.assertTrue(TestHelper.shortcutTitle.isEnabled());
        Assert.assertEquals(TestHelper.shortcutTitle.getText(), "gigantic experience");
        Assert.assertTrue(TestHelper.AddtoHSOKBtn.isEnabled());
        Assert.assertTrue(TestHelper.AddtoHSCancelBtn.isEnabled());

        //Edit shortcut text
        TestHelper.shortcutTitle.click();
        TestHelper.shortcutTitle.setText("For Testing Purpose");
        TestHelper.AddtoHSOKBtn.click();

        // For Android O, we need additional steps
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog();
        }

        //App is sent to background, in launcher now
        shortcutIcon.waitForExists(waitingTime);
        Assert.assertTrue(shortcutIcon.isEnabled());
        shortcutIcon.click();
        TestHelper.browserURLbar.waitForExists(waitingTime);
        Assert.assertTrue(
                TestHelper.browserURLbar.getText()
                        .equals(webServer.url(TEST_PATH).toString()));
    }

    @Test
    public void NonameTest() throws InterruptedException, UiObjectNotFoundException, IOException {
        UiObject shortcutIcon = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .description("localhost")
                .enabled(true));

        removeWelcomeOverlay();

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.progressBar.waitForExists(webPageLoadwaitingTime);
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime));
        Assert.assertTrue("Website title loaded", titleMsg.exists());

        TestHelper.menuButton.perform(click());
        // If the menu item is not clickable, wait and retry
        while (!TestHelper.AddtoHSmenuItem.waitForExists(waitingTime)) {
            TestHelper.pressBackKey();
            Thread.sleep(waitingTime);
            TestHelper.menuButton.perform(click());
        }
        TestHelper.AddtoHSmenuItem.click();

        // Add to Home screen dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime);

        Assert.assertTrue(TestHelper.shortcutTitle.isEnabled());
        Assert.assertEquals(TestHelper.shortcutTitle.getText(), "gigantic experience");
        Assert.assertTrue(TestHelper.AddtoHSOKBtn.isEnabled());
        Assert.assertTrue(TestHelper.AddtoHSCancelBtn.isEnabled());

        //remove shortcut text
        TestHelper.shortcutTitle.click();
        TestHelper.shortcutTitle.setText("");
        TestHelper.AddtoHSOKBtn.click();

        // For Android O, we need additional steps
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog();
        }

        //App is sent to background, in launcher now
        shortcutIcon.waitForExists(waitingTime);
        Assert.assertTrue(shortcutIcon.isEnabled());
        shortcutIcon.click();
        TestHelper.browserURLbar.waitForExists(waitingTime);
        Assert.assertTrue(
                TestHelper.browserURLbar.getText()
                        .equals(webServer.url(TEST_PATH).toString()));
    }

    @Test
    public void SearchTermShortcutTest() throws InterruptedException, UiObjectNotFoundException, IOException {
        UiObject shortcutIcon = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .descriptionContains("helloworld")
                .enabled(true));

        removeWelcomeOverlay();

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("helloworld");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.progressBar.waitForExists(webPageLoadwaitingTime);
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime));

        TestHelper.menuButton.perform(click());
        // If the menu item is not clickable, wait and retry
        while (!TestHelper.AddtoHSmenuItem.waitForExists(waitingTime)) {
            TestHelper.pressBackKey();
            Thread.sleep(waitingTime);
            TestHelper.menuButton.perform(click());
        }
        TestHelper.AddtoHSmenuItem.click();

        // Add to Home screen dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime);
        TestHelper.AddtoHSOKBtn.click();

        // For Android O, we need additional steps
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog();
        }

        //App is sent to background, in launcher now
        shortcutIcon.waitForExists(waitingTime);
        Assert.assertTrue(shortcutIcon.isEnabled());
        shortcutIcon.click();
        TestHelper.waitForIdle();
        TestHelper.webView.waitForExists(waitingTime);

        //Tap URL bar and check the search term is still shown
        TestHelper.browserURLbar.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.browserURLbar.getText().contains("helloworld"));
    }
}
