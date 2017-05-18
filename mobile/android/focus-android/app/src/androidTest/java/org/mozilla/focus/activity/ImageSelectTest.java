/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.os.Build;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;
import android.view.KeyEvent;

import junit.framework.Assert;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.action.ViewActions.click;
import static org.junit.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;


@RunWith(AndroidJUnit4.class)
public class ImageSelectTest {
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
    private UiObject titleMsg = TestHelper.mDevice.findObject(new UiSelector()
            .description("focus test page")
            .enabled(true));

    private UiObject rabbitImage = TestHelper.mDevice.findObject(new UiSelector()
            .description("Smiley face")
            .enabled(true));
    private UiObject imageMenuTitle = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/title")
            .enabled(true));
    private UiObject shareMenu = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/design_menu_item_text")
            .text("Share image")
            .enabled(true));
    private UiObject copyMenu = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/design_menu_item_text")
            .text("Copy image address")
            .enabled(true));
    private UiObject saveMenu = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/design_menu_item_text")
            .text("Save image")
            .enabled(true));

    @Test
    public void ImageMenuTest() throws InterruptedException, UiObjectNotFoundException, IOException {
        final long waitingTime = TestHelper.waitingTime;
        final String imagePath = webServer.url(TEST_PATH).toString() + "rabbit.jpg";

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
        assertTrue("Website title loaded", titleMsg.exists());

        // Find image and long tap it
        Assert.assertTrue(rabbitImage.exists());
        rabbitImage.dragTo(rabbitImage,5);
        imageMenuTitle.waitForExists(waitingTime);
        Assert.assertTrue(imageMenuTitle.exists());
        Assert.assertEquals(imageMenuTitle.getText(), webServer.url(TEST_PATH).toString() + "rabbit.jpg");
        Assert.assertTrue(shareMenu.exists());
        Assert.assertTrue(copyMenu.exists());
        Assert.assertTrue(saveMenu.exists());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            copyMenu.click();
        }
        else {
            TestHelper.mDevice.pressBack();
        }

        // Erase browsing session
        TestHelper.floatingEraseButton.perform(click());
        TestHelper.erasedMsg.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.erasedMsg.exists());
        Assert.assertTrue(TestHelper.urlBar.exists());


        // KeyEvent.KEYCODE_PASTE) requires API 24 or above
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            TestHelper.urlBar.click();
            TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
            TestHelper.mDevice.pressKeyCode(KeyEvent.KEYCODE_PASTE);
            Assert.assertEquals(TestHelper.inlineAutocompleteEditText.getText(), imagePath);
        }
    }
}