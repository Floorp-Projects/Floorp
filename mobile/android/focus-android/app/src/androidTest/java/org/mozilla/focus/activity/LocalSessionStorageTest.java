/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.preference.PreferenceManager;
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

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.action.ViewActions.click;
import static org.junit.Assert.assertTrue;
import static org.mozilla.focus.activity.TestHelper.waitingTime;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

@RunWith(AndroidJUnit4.class)
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
    public void tearDown() throws Exception {
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
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
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
