/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers;

import android.content.Context;
import android.content.res.Resources;
import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.ViewInteraction;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;
import android.text.format.DateUtils;
import android.util.DisplayMetrics;

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

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.espresso.web.sugar.Web.onWebView;
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
        assertTrue(geckoView.waitForExists(waitingTime));
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
    public static UiObject permAllowBtn = mDevice.findObject(new UiSelector()
            .resourceId("com.android.packageinstaller:id/permission_allow_button")
            .clickable(true));
    public static UiObject inlineAutocompleteEditText = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/urlView")
            .focused(true)
            .enabled(true));
    public static UiObject searchSuggestionsTitle = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/enable_search_suggestions_title")
            .enabled(true));
    public static UiObject searchSuggestionsButtonYes = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/enable_search_suggestions_button")
            .enabled(true));
    public static UiObject cleartextField = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/clearView")
            .enabled(true));
    public static UiObject hint = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/searchView")
            .clickable(true));
    public static UiObject suggestionList = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/suggestionList"));
    public static UiObject suggestion = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/suggestion"));
    public static UiObject webView = mDevice.findObject(new UiSelector()
            .className("android.webkit.WebView")
            .enabled(true));
    public static UiObject geckoView = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/webview")
            .enabled(true));
    public static UiObject progressBar = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/progress")
            .enabled(true));
    public static UiObject tryAgainBtn = mDevice.findObject(new UiSelector()
            .resourceId("errorTryAgain")
            .clickable(true));
    public static ViewInteraction floatingEraseButton = onView(
            allOf(withId(R.id.erase), isDisplayed()));
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
            .className("android.widget.Button")
            .instance(1)
            .enabled(true));
    public static UiObject shortcutTitle = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/edit_title")
            .enabled(true));
    public static UiObject savedNotification = TestHelper.mDevice.findObject(new UiSelector()
            .text("Download complete.")
            .resourceId("android:id/text")
            .enabled(true));

    public static UiObject securityInfoIcon = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/security_info")
            .enabled(true));
    public static UiObject identityState = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/site_identity_state")
            .enabled(true));

    public static UiObject downloadTitle = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/title_template")
            .enabled(true));
    public static UiObject downloadFileName = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/download_dialog_file_name")
            .enabled(true));
    public static UiObject downloadWarning = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/download_dialog_warning")
            .enabled(true));
    public static UiObject downloadCancelBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/download_dialog_cancel")
            .enabled(true));
    public static UiObject downloadBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/download_dialog_download")
            .enabled(true));
    public static UiObject completedMsg = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId(TestHelper.getAppName() + ":id/snackbar_text")
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
    public static UiObject settingsMenu = mDevice.findObject(new UiSelector()
            .resourceId(getAppName() + ":id/recycler_view"));
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

    public static void expandNotification() throws UiObjectNotFoundException {
        if (!notificationOpenItem.waitForExists(waitingTime)) {
            if (!notificationExpandSwitch.exists()) {
                notificationBarDeleteItem.pinchOut(50, 5);
            } else {
                notificationExpandSwitch.click();
            }
            assertTrue(notificationOpenItem.exists());
        }
    }

    public static final int X_OFFSET = 20;
    public static final int Y_OFFSET = 500;
    public static final int STEPS = 10;

    private static DisplayMetrics devicePixels() {
        DisplayMetrics metrics = Resources.getSystem().getDisplayMetrics();
        return metrics;
    }

    public static void swipeScreenLeft() {
        DisplayMetrics metrics = devicePixels();
        mDevice.swipe(metrics.widthPixels - X_OFFSET, Y_OFFSET, 0, Y_OFFSET, STEPS);
    }

    public static void swipeScreenRight() {
        DisplayMetrics metrics = devicePixels();
        mDevice.swipe(X_OFFSET, Y_OFFSET, metrics.widthPixels, Y_OFFSET, STEPS);
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
    public static void openNotification() {
        mDevice.openNotification();
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
        onWebView(withText("focus test page"));
    }

    public static void selectGeckoForKlar() {
        InstrumentationRegistry.getTargetContext().getSharedPreferences("mozilla.components.service.fretboard.overrides",
                Context.MODE_PRIVATE)
                .edit()
                .putBoolean("use-gecko", AppConstants.INSTANCE.isKlarBuild())
                .commit();
    }
}
