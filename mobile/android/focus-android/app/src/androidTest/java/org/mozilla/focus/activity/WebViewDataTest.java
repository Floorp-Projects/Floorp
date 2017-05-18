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
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;

import junit.framework.Assert;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import okhttp3.mockwebserver.RecordedRequest;

import static android.support.test.espresso.action.ViewActions.click;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

/**
 * This test browses to a test site and makes sure that no traces are left on disk.
 *
 * The test uses a whitelist so it might fail as soon as you store new files on disk.
 */
@RunWith(AndroidJUnit4.class)
public class WebViewDataTest {
    private static final List<String> WHITELIST = Arrays.asList(
            "cache", // We assert that this folder is empty
            "code_cache",
            "shared_prefs",
            "app_dxmaker_cache",
            "telemetry",
            "databases"
    );

    private static final String TEST_PATH = "/copper/truck/destroy?smoke=violet#bizarre";

    /**
     * If any of those strings show up in local files then we consider the browsing session to be leaked.
     */
    private static final List<String> TEST_TRACES = Arrays.asList(
            // URL fragments
            "localhost",
            "copper",
            "truck",
            "destroy",
            "smoke",
            "violet",
            "bizarre",
            // Content from the test page
            "groovy",
            "rabbits",
            "gigantic",
            "experience",
            // Content from cookies
            "birthday",
            "armchair",
            "sphere",
            "battery",
            // Content from service worker
            "KANGAROO"
    );

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
                        .setBody(TestHelper.readTestAsset("test.html"))
                        .addHeader("Set-Cookie", "sphere=battery; Expires=Wed, 21 Oct 2035 07:28:00 GMT;"));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("rabbit.jpg")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("service-worker.js"))
                        .setHeader("Content-Type", "text/javascript"));
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

    @Test
    public void DeleteWebViewDataTest() throws InterruptedException, UiObjectNotFoundException, IOException {
        final long waitingTime = TestHelper.waitingTime;

        // Load website with service worker

        TestHelper.urlBar.waitForExists(waitingTime);
        TestHelper.urlBar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.webView.waitForExists(waitingTime);

        // Assert website is loaded

        final UiObject titleMsg = TestHelper.mDevice.findObject(new UiSelector()
                .description("focus test page")
                .enabled(true));
        titleMsg.waitForExists(waitingTime);
        assertTrue("Website title loaded", titleMsg.exists());

        // Assert cookie is saved

        final UiObject cookieMsg = TestHelper.mDevice.findObject(new UiSelector()
                .description("Cookie saved")
                .enabled(true));
        cookieMsg.waitForExists(waitingTime);
        assertTrue("Cookie is saved", cookieMsg.exists());

        final UiObject serviceWorkerMsg = TestHelper.mDevice.findObject(new UiSelector()
                .description("Service worker installed")
                .enabled(true));
        serviceWorkerMsg.waitForExists(waitingTime);
        assertTrue("Service worker installed", serviceWorkerMsg.exists());

        // Erase browsing session

        TestHelper.floatingEraseButton.perform(click());
        TestHelper.erasedMsg.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.erasedMsg.exists());
        Assert.assertTrue(TestHelper.urlBar.exists());
        TestHelper.waitForIdle();

        // Make sure all resources have been loaded from the web server

        assertPathsHaveBeenRequested(webServer,
                "/copper/truck/destroy?smoke=violet", // Actual URL
                "/copper/truck/rabbit.jpg", // Our service worker is installed
                "/copper/truck/service-worker.js"); // Our service worker is installed

        // Now let's assert that there are no surprises in the data directory

        final File dataDir = new File(appContext.getApplicationInfo().dataDir);
        assertTrue("App data directory should exist", dataDir.exists());

        final File webViewDirectory = new File(dataDir, "app_webview");
        assertFalse("WebView directory should not exist", webViewDirectory.exists());

        assertIsEmpty(appContext.getCacheDir());

        for (final String name : dataDir.list()) {
            assertTrue("Check '" + name + "' is in whitelist", WHITELIST.contains(name));
        }

        assertNoTraces(dataDir);
    }

    private void assertPathsHaveBeenRequested(MockWebServer webServer, String... paths) throws InterruptedException {
        final List<String> expectedPaths = new ArrayList<>(paths.length);
        Collections.addAll(expectedPaths, paths);

        RecordedRequest request;

        while ((request = webServer.takeRequest(TestHelper.waitingTime, TimeUnit.MILLISECONDS)) != null) {
            if (!expectedPaths.remove(request.getPath())) {
                throw new AssertionError("Unknown path requested: " + request.getPath());
            }
        }

        if (!expectedPaths.isEmpty()) {
            throw new AssertionError("Expected paths not requested: " + expectedPaths);
        }
    }

    private void assertNoTraces(File directory) throws IOException {
        for (final String name : directory.list()) {
            final File file = new File(directory, name);

            if (file.isDirectory()) {
                assertNoTraces(file);
                continue;
            }

            final String content = TestHelper.readFileToString(file);
            for (String trace : TEST_TRACES) {
                assertFalse("File '" + name + "' should not contain any traces of browser session (" + trace + ", path=" + file.getAbsolutePath() + ")",
                        content.contains(trace));
            }
        }
    }

    private void assertIsEmpty(File directory) {
        assertTrue(directory.isDirectory() && directory.list().length == 0);
    }


}
