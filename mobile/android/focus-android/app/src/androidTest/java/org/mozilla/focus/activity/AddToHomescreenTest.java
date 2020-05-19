/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.os.Build;

import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.helpers.TestHelper;
import org.mozilla.focus.utils.AppConstants;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static androidx.test.espresso.action.ViewActions.click;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;
import static org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime;

// https://testrail.stage.mozaws.net/index.php?/cases/view/60852
// includes:
// https://testrail.stage.mozaws.net/index.php?/cases/view/40066
@RunWith(AndroidJUnit4.class)
public class AddToHomescreenTest {
    private static final String TEST_PATH = "/";
    private MockWebServer webServer;
    private int webServerPort;
    private String webServerBookmarkName;

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule  = new ActivityTestRule<MainActivity>(MainActivity.class) {
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
            TestHelper.selectGeckoForKlar();

            webServer = new MockWebServer();
            // note: requesting getPort() will automatically start the mock server,
            //       so if you use the 2 lines, do not try to start server or it will choke.
            webServerPort = webServer.getPort();
            webServerBookmarkName = "localhost_" + Integer.toString(webServerPort);

            try {
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("plain_test.html")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("plain_test.html")));
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
    public void tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    private UiObject welcomeBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("com.android.launcher3:id/cling_dismiss_longpress_info")
            .text("GOT IT")
            .enabled(true));

    private void handleShortcutLayoutDialog() throws UiObjectNotFoundException {
        TestHelper.AddautoBtn.waitForExists(waitingTime);
        TestHelper.AddautoBtn.click();
        TestHelper.AddautoBtn.waitUntilGone(waitingTime);
        TestHelper.pressHomeKey();
    }

    private void openAddtoHSDialog() throws UiObjectNotFoundException {
        TestHelper.menuButton.perform(click());
        TestHelper.AddtoHSmenuItem.waitForExists(waitingTime);
        // If the menu item is not clickable, wait and retry
        while (!TestHelper.AddtoHSmenuItem.isClickable()) {
            TestHelper.pressBackKey();
            TestHelper.menuButton.perform(click());
        }
        TestHelper.AddtoHSmenuItem.click();
    }

    @Test
    public void AddToHomeScreenTest() throws UiObjectNotFoundException {

        UiObject shortcutIcon = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .description(webServerBookmarkName)
                .enabled(true));

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.progressBar.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime));

        openAddtoHSDialog();
        // Add to Home screen dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime);

        Assert.assertTrue(TestHelper.shortcutTitle.isEnabled());
        Assert.assertEquals(TestHelper.shortcutTitle.getText(), "gigantic experience");
        Assert.assertTrue(TestHelper.AddtoHSOKBtn.isEnabled());
        Assert.assertTrue(TestHelper.AddtoHSCancelBtn.isEnabled());

        //Edit shortcut text
        TestHelper.shortcutTitle.click();
        TestHelper.shortcutTitle.setText(webServerBookmarkName);
        TestHelper.AddtoHSOKBtn.click();

        // For Android O, we need additional steps
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog();
        }

        //App is sent to background, in launcher now
        if (welcomeBtn.exists()) {
            welcomeBtn.click();
        }
        shortcutIcon.waitForExists(waitingTime);
        Assert.assertTrue(shortcutIcon.isEnabled());
        shortcutIcon.click();
        TestHelper.browserURLbar.waitForExists(waitingTime);
        Assert.assertTrue(
                TestHelper.browserURLbar.getText()
                        .equals(webServer.url(TEST_PATH).toString()));
    }

    @Test
    public void NonameTest() throws UiObjectNotFoundException {
        UiObject shortcutIcon = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .description(webServerBookmarkName)
                .enabled(true));

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.progressBar.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime));

        openAddtoHSDialog();

        // "Add to Home screen" dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime);

        Assert.assertTrue(TestHelper.shortcutTitle.isEnabled());
        Assert.assertEquals(TestHelper.shortcutTitle.getText(), "gigantic experience");
        Assert.assertTrue(TestHelper.AddtoHSOKBtn.isEnabled());
        Assert.assertTrue(TestHelper.AddtoHSCancelBtn.isEnabled());

        //replace shortcut text
        TestHelper.shortcutTitle.click();
        TestHelper.shortcutTitle.setText(webServerBookmarkName);
        TestHelper.AddtoHSOKBtn.click();

        // For Android O, we need additional steps
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog();
        }
        if (welcomeBtn.exists()) {
            welcomeBtn.click();
        }
        //App is sent to background, in launcher now
        //Start from home and then swipe, to ensure we land where we want to search for shortcut
        TestHelper.mDevice.pressHome();
        TestHelper.swipeScreenLeft();
        shortcutIcon.waitForExists(waitingTime);
        Assert.assertTrue(shortcutIcon.isEnabled());
        shortcutIcon.click();
        TestHelper.browserURLbar.waitForExists(waitingTime);
        Assert.assertTrue(
                TestHelper.browserURLbar.getText()
                        .equals(webServer.url(TEST_PATH).toString()));
    }

    @Test
    public void SearchTermShortcutTest() throws UiObjectNotFoundException {
        UiObject shortcutIcon = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .descriptionContains("helloworld")
                .enabled(true));

        // Open website, and click 'Add to homescreen'
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("helloworld");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        // In certain cases, where progressBar disappears immediately, below will return false
        // since it busy waits, it will unblock when the bar isn't visible, regardless of the
        // return value
        TestHelper.progressBar.waitForExists(waitingTime);
        TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime);

        openAddtoHSDialog();

        // "Add to Home screen" dialog is now shown
        TestHelper.shortcutTitle.waitForExists(waitingTime);
        TestHelper.AddtoHSOKBtn.click();

        // For Android O, we need additional steps
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            handleShortcutLayoutDialog();
        }

        if (welcomeBtn.exists()) {
            welcomeBtn.click();
        }

        //App is sent to background, in launcher now
        //Start from home and then swipe, to ensure we land where we want to search for shortcut
        TestHelper.mDevice.pressHome();
        TestHelper.swipeScreenLeft();
        shortcutIcon.waitForExists(waitingTime);
        Assert.assertTrue(shortcutIcon.isEnabled());
        shortcutIcon.click();
        TestHelper.waitForIdle();
        TestHelper.waitForWebContent();

        //Tap URL bar and check the search term is still shown
        TestHelper.browserURLbar.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.browserURLbar.getText().contains("helloworld"));
    }
}
