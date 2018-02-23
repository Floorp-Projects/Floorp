/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers;

import android.os.RemoteException;
import android.support.annotation.NonNull;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.ViewInteraction;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;
import android.text.format.DateUtils;

import org.junit.Assert;
import org.mozilla.focus.R;
import org.mozilla.focus.utils.AppConstants;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

import okhttp3.mockwebserver.MockResponse;
import okio.Buffer;
import okio.Okio;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.view.KeyEvent.KEYCODE_ENTER;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.allOf;

// This test visits each page and checks whether some essential elements are being displayed
public final class TestHelper {

    public static UiDevice mDevice =  UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    public static final long waitingTime = DateUtils.SECOND_IN_MILLIS * 4;
    public static final long webPageLoadwaitingTime = DateUtils.SECOND_IN_MILLIS * 15;

    public static String getAppName() {
        return InstrumentationRegistry.getInstrumentation()
                .getTargetContext().getPackageName();
    }

    // wait for web area to be visible
    public static void waitForWebContent() {
        if (!AppConstants.isGeckoBuild()) {
            assertTrue(webView.waitForExists(waitingTime));
        } else {
            assertTrue(geckoView.waitForExists(waitingTime));
        }
    }

    /********* First View Locators ***********/
    public static UiObject firstSlide = mDevice.findObject(new UiSelector()
            .text("Power up your privacy")
            .enabled(true));
    public static UiObject secondSlide = mDevice.findObject(new UiSelector()
            .text("Your search, your way")
            .enabled(true));
    public static UiObject thirdSlide = mDevice.findObject(new UiSelector()
            .text("Add shortcuts to your home screen")
            .enabled(true));
    public static UiObject lastSlide = mDevice.findObject(new UiSelector()
            .text("Make privacy a habit")
            .enabled(true));
    public static UiObject nextBtn = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/next")
            .enabled(true));
    public static UiObject finishBtn = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/finish")
            .enabled(true));
    public static UiObject initialView = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/backgroundView")
            .enabled(true));

    /********* Main View Locators ***********/
    public static ViewInteraction menuButton = onView(
            allOf(withId(R.id.menuView),
                    isDisplayed()));

    /********* Web View Locators ***********/
    public static UiObject browserURLbar = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/display_url")
            .clickable(true));

    public static UiObject inlineAutocompleteEditText = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/urlView")
            .focused(true)
            .enabled(true));
    public static UiObject cleartextField = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/clearView")
            .enabled(true));
    public static UiObject hint = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/searchView")
            .clickable(true));
    public static UiObject webView = mDevice.findObject(new UiSelector()
            .className("android.webkit.WebView")
            .enabled(true));
    public static UiObject geckoView = mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.klar.gecko.debug:id/webview")
            .enabled(true));
    public static UiObject progressBar = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/progress")
            .enabled(true));
    public static UiObject tryAgainBtn = mDevice.findObject(new UiSelector()
            .description("Try Again")
            .clickable(true));
    public static ViewInteraction floatingEraseButton = onView(
            allOf(withId(R.id.erase), isDisplayed()));
    public static UiObject notFoundMsg = mDevice.findObject(new UiSelector()
            .description("The address wasn’t understood")
            .enabled(true));
    public static UiObject notFounddetailedMsg = mDevice.findObject(new UiSelector()
            .description("You might need to install other software to open this address.")
            .enabled(true));
    public static UiObject browserViewSettingsMenuItem = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/settings")
            .clickable(true));
    public static UiObject erasedMsg = TestHelper.mDevice.findObject(new UiSelector()
            .text("Your browsing history has been erased.")
            .resourceId(getAppName() + ":id/snackbar_text")
            .enabled(true));
    public static UiObject lockIcon = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/lock")
            .description("Secure connection"));
    public static UiObject notificationBarDeleteItem = TestHelper.mDevice.findObject(new UiSelector()
            .text("Erase browsing history")
            .resourceId("android:id/text")
            .enabled(true));
    public static UiObject notificationExpandSwitch = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("android:id/expand_button")
            .packageName(getAppName() + "")
            .enabled(true));
    public static UiObject notificationOpenItem = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("android:id/action0")
            .description("Open")
            .enabled(true));
    public static UiObject notificationEraseOpenItem = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("android:id/action0")
            .description("Erase and Open")
            .enabled(true));
    public static UiObject FocusInRecentApps = TestHelper.mDevice.findObject(new UiSelector()
            .text("Focus (Dev)")
            .resourceId("com.android.systemui:id/title")
            .enabled(true));
    public static UiObject blockOffIcon = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/block")
            .enabled(true));
    public static UiObject AddtoHSmenuItem = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/add_to_homescreen")
            .enabled(true));
    public static UiObject AddtoHSCancelBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/addtohomescreen_dialog_cancel")
            .enabled(true));
    public static UiObject AddtoHSOKBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/addtohomescreen_dialog_add")
            .enabled(true));
    public static UiObject AddautoBtn = TestHelper.mDevice.findObject(new UiSelector()
            .text("ADD AUTOMATICALLY")
            .enabled(true));
    public static UiObject shortcutTitle = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/edit_title")
            .enabled(true));
    public static UiObject savedNotification = TestHelper.mDevice.findObject(new UiSelector()
            .text("Download complete.")
            .resourceId("android:id/text")
            .enabled(true));


    /********* Main View Menu Item Locators ***********/
    public static UiObject whatsNewItem = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/whats_new")
            .enabled(true));
    public static UiObject HelpItem = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/help")
            .enabled(true));
    public static UiObject settingsMenuItem = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/settings")
            .enabled(true));
    public static UiObject blockCounterItem = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/trackers_count"));
    public static UiObject blockToggleSwitch = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/blocking_switch"));
    public static UiObject menulist = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/list")
            .enabled(true));
    public static String getMenuItemText(UiObject item) throws UiObjectNotFoundException {
        return item.getChild(new UiSelector().index(0))
                .getChild(new UiSelector().index(0)).getText();
    }

    /********** Share Menu Dialog ********************/
    public static UiObject shareMenuHeader = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("android:id/title")
            .text("Share via")
            .enabled(true));
    public static UiObject shareAppList = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("android:id/resolver_list")
            .enabled(true));

    /********* Settings Menu Item Locators ***********/
    public static UiScrollable settingsList = new UiScrollable(new UiSelector()
            .resourceId("android:id/list").scrollable(true));
    public static UiObject settingsHeading = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/toolbar")
            .enabled(true));
    public static UiObject navigateUp = mDevice.findObject(new UiSelector()
            .description("Navigate up"));
    public static UiObject toggleAnalyticBlock = mDevice.findObject(new UiSelector()
            .className("android.widget.Switch")
            .instance(1));
    public static UiObject refreshBtn = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/refresh")
            .enabled(true));

    private TestHelper () throws UiObjectNotFoundException {
    }

    public static void waitForIdle() {
        mDevice.waitForIdle(waitingTime);
    }
    public static void pressEnterKey() {
        mDevice.pressKeyCode(KEYCODE_ENTER);
    }
    public static void pressBackKey() {
        mDevice.pressBack();
    }
    public static void pressHomeKey() {
        mDevice.pressHome();
    }
    public static void pressRecentAppsKey() throws RemoteException {
        mDevice.pressRecentApps();
    }
    public static void openNotification() {
        mDevice.openNotification();
    }

    public static void swipeUpScreen () {
        int dHeight = mDevice.getDisplayHeight();
        int dWidth = mDevice.getDisplayWidth();
        int xScrollPosition = dWidth / 2;
        int yScrollStart = dHeight / 4 * 3;
        mDevice.swipe(
                xScrollPosition,
                yScrollStart,
                xScrollPosition,
                0,
                20
        );
    }

    public static void swipedownScreen () {
        int dHeight = mDevice.getDisplayHeight();
        int dWidth = mDevice.getDisplayWidth();
        int xScrollPosition = dWidth / 2;
        int yScrollStart = dHeight / 4;
        mDevice.swipe(
                xScrollPosition,
                yScrollStart,
                xScrollPosition,
                dHeight,
                20
        );
    }

    public static MockResponse createMockResponseFromAsset(@NonNull String fileName) throws IOException {
        return new MockResponse()
                .setBody(TestHelper.readTestAsset(fileName));
    }

    public static Buffer readTestAsset(String filename) throws IOException {
        try (final InputStream stream = InstrumentationRegistry.getContext().getAssets().open(filename)) {
            return readStreamFile(stream);
        }
    }

    public static Buffer readStreamFile(InputStream file) throws IOException {

        Buffer buffer = new Buffer();
        buffer.writeAll(Okio.source(file));
        return buffer;
    }

    public static  String readFileToString(File file) throws IOException {
        System.out.println("Reading file: " + file.getAbsolutePath());

        try (final FileInputStream stream = new FileInputStream(file)) {
            return readStreamIntoString(stream);
        }
    }

    public static  String readStreamIntoString(InputStream stream) throws IOException {
        try (final BufferedReader reader = new BufferedReader(
                new InputStreamReader(stream, StandardCharsets.UTF_8))) {

            final StringBuilder builder = new StringBuilder();
            String line;

            while ((line = reader.readLine()) != null) {
                builder.append(line);
            }
            reader.close();

            return builder.toString();
        }
    }

    public static void waitForWebSiteTitleLoad() {
        UiObject titleMsg = mDevice.findObject(new UiSelector()
                .description("focus test page")
                .enabled(true));

        assertTrue(webView.waitForExists(waitingTime));
        Assert.assertTrue("Website title loaded", titleMsg.waitForExists(waitingTime));
    }
}
