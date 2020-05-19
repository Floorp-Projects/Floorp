/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.privacy;

import android.content.Context;

import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiSelector;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.helpers.TestHelper;
import org.mozilla.focus.utils.AppConstants;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static androidx.test.espresso.action.ViewActions.click;
import static org.junit.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

@RunWith(AndroidJUnit4.class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
public class LocalSessionStorageTest {
    public static final String SESSION_STORAGE_HIT = "Session storage has value";
    public static final String LOCAL_STORAGE_MISS = "Local storage empty";
    private MockWebServer webServer;

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

            webServer = new MockWebServer();

            try {
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("storage_start.html")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("storage_check.html")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("storage_check.html")));
                webServer.start();
            } catch (IOException e) {
                throw new AssertionError("Could not start web server", e);
            }
        }

        @Override
        protected void afterActivityFinished() {
            super.afterActivityFinished();

            try {
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

    /**
     * Make sure that session storage values are kept and written but removed at the end of a session.
     */
    @Test
    public void testLocalAndSessionStorageIsWrittenAndRemoved() throws Exception {
        // Go to first website that saves a value in the session/local store.

        goToUrlFromHomeScreen("/sessionStorage_start");

        // Assert website is loaded and values are written.

        final UiObject savedMsg = TestHelper.mDevice.findObject(new UiSelector()
                .description("Values written to storage")
                .enabled(true));
        savedMsg.waitForExists(waitingTime);
        assertTrue("Website loaded and values written to local / session storage", savedMsg.exists());

        // Now load the next website and assert that the values are still in the storage

        goToUrlFromBrowserScreen("/sessionStorage_check");

        { // Value is still stored in session storage
            final UiObject sessionStorageMsg = TestHelper.mDevice.findObject(new UiSelector()
                    .description(SESSION_STORAGE_HIT)
                    .enabled(true));
            sessionStorageMsg.waitForExists(waitingTime);
            assertTrue(sessionStorageMsg.exists());
        }
        { // Local storage is empty
            final UiObject localStorageMsg = TestHelper.mDevice.findObject(new UiSelector()
                    .description(LOCAL_STORAGE_MISS)
                    .enabled(true));
            localStorageMsg.waitForExists(waitingTime);
            assertTrue(localStorageMsg.exists());
        }

        // Let's press the "erase" button and end the session

        TestHelper.floatingEraseButton.perform(click());
        TestHelper.erasedMsg.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.erasedMsg.exists());
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists());

        // Now go to the website again and make sure that both values are not in the session/local store

        goToUrlFromHomeScreen("/sessionStorage_check");

        { // Session storage is empty
            final UiObject sessionStorageMsg = TestHelper.mDevice.findObject(new UiSelector()
                    .description("Session storage empty")
                    .enabled(true));
            sessionStorageMsg.waitForExists(waitingTime);
            assertTrue(sessionStorageMsg.exists());
        }
        { // Local storage is empty
            final UiObject localStorageMsg = TestHelper.mDevice.findObject(new UiSelector()
                    .description("Local storage empty")
                    .enabled(true));
            localStorageMsg.waitForExists(waitingTime);
            assertTrue(localStorageMsg.exists());
        }

    }

    private void goToUrlFromHomeScreen(String url) throws Exception {
        // Load website with service worker
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(url).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebContent();
    }

    private void goToUrlFromBrowserScreen(String url) throws Exception {
        // Load website with service worker
        TestHelper.browserURLbar.waitForExists(waitingTime);
        TestHelper.browserURLbar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(url).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
    }
}
