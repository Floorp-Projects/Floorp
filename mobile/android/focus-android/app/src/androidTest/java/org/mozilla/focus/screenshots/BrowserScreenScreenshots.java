/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.os.SystemClock;

import androidx.test.espresso.NoMatchingViewException;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;
import androidx.test.uiautomator.Until;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import tools.fastlane.screengrab.Screengrab;
import tools.fastlane.screengrab.locale.LocaleTestRule;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasFocus;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withResourceName;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.mozilla.focus.helpers.EspressoHelper.openSettings;

@Ignore("See: https://github.com/mozilla-mobile/mobile-test-eng/issues/305")
@RunWith(AndroidJUnit4.class)
public class BrowserScreenScreenshots extends ScreenshotTest {


    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    private MockWebServer webServer;

    @Before
    public void setUpWebServer() throws IOException {
        webServer = new MockWebServer();

        // Test page
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("image_test.html")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("rabbit.jpg")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("download.jpg")));
        // Download
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("image_test.html")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("rabbit.jpg")));
        webServer.enqueue(new MockResponse()
                .setBody(TestHelper.readTestAsset("download.jpg")));
    }

    @After
    public void tearDownWebServer() {
        try {
            webServer.close();
            webServer.shutdown();
        } catch (IOException e) {
            throw new AssertionError("Could not stop web server", e);
        }
    }

    @Test
    public void takeScreenshotsOfBrowsingScreen() throws Exception {
        SystemClock.sleep(5000);
        takeScreenshotsOfBrowsingView();
        takeScreenshotsOfOpenWithAndShare();
        takeAddToHomeScreenScreenshot();
        takeScreenshotofInsecureCon();
        takeScreenshotOfFindDialog();
        takeScreenshotOfTabsTrayAndErase();
        takeScreenshotofSecureCon();
    }

    private void takeScreenshotsOfBrowsingView() {
        onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
                .check(matches(isDisplayed()));

        // click yes, then go into search dialog and change to twitter, or create twitter engine
        // If it does not exist (In order to get search unavailable dialog)
        openSettings();
        onView(withText(R.string.preference_category_search))
                .perform(click());
        onView(allOf(withText(R.string.preference_search_engine_label),
                withResourceName("title")))
                .perform(click());
        onView(withText(R.string.preference_search_installed_search_engines))
                .check(matches(isDisplayed()));

       try {
           onView(withText("Twitter"))
                   .check(matches(isDisplayed()))
                   .perform(click());
       } catch (NoMatchingViewException doesnotexist) {
           final String addEngineLabel = getString(R.string.preference_search_add2);
           onView(withText(addEngineLabel))
                   .check(matches(isEnabled()))
                   .perform(click());
           onView(withId(R.id.edit_engine_name))
                   .check(matches(isEnabled()));
           onView(withId(R.id.edit_engine_name))
                   .perform(replaceText("twitter"));
           onView(withId(R.id.edit_search_string))
                   .perform(replaceText("https://twitter.com/search?q=%s"));
           onView(withId(R.id.menu_save_search_engine))
                   .check(matches(isEnabled()))
                   .perform(click());
       }

        device.pressBack();
        onView(allOf(withText(R.string.preference_search_engine_label),
                withResourceName("title")))
                .check(matches(isDisplayed()));
        device.pressBack();
        onView(withText(R.string.preference_category_search))
                .check(matches(isDisplayed()));
        device.pressBack();

        onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText(webServer.url("/").toString()));
        try {
            onView(withId(R.id.enable_search_suggestions_button))
                    .check(matches(isDisplayed()));
            Screengrab.screenshot("Enable_Suggestion_dialog");
            onView(withId(R.id.enable_search_suggestions_button))
                    .perform(click());
            Screengrab.screenshot("Suggestion_unavailable_dialog");
            onView(withId(R.id.dismiss_no_suggestions_message))
                    .perform(click());
        } catch (AssertionError dne) { }

        onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(pressImeActionButton());

        device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/webview")
                .enabled(true))
                .waitForExists(waitingTime);

        onView(withId(R.id.mozac_browser_toolbar_url_view))
                .check(matches(isDisplayed()))
                .check(matches(withText(containsString(webServer.getHostName()))));
    }

    private void takeScreenshotsOfOpenWithAndShare() throws Exception {
        /* Open_With View */
        TestHelper.menuButton.perform(click());

        UiObject openWithBtn = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/open_select_browser")
                .enabled(true));
        assertTrue(openWithBtn.waitForExists(waitingTime));
        openWithBtn.click();
        UiObject shareList = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/apps")
                .enabled(true));
        assertTrue(shareList.waitForExists(waitingTime));
        Screengrab.screenshot("OpenWith_Dialog");

        /* Share View */
        UiObject shareBtn = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/share")
                .enabled(true));
        device.pressBack();
        TestHelper.menuButton.perform(click());
        assertTrue(shareBtn.waitForExists(waitingTime));
        shareBtn.click();
        TestHelper.shareAppList.waitForExists(waitingTime);
        Screengrab.screenshot("Share_Dialog");

        device.pressBack();
    }

    private void takeAddToHomeScreenScreenshot() throws UiObjectNotFoundException {
        TestHelper.menuButton.perform(click());

        TestHelper.AddtoHSmenuItem.waitForExists(waitingTime);
        TestHelper.AddtoHSmenuItem.click();

        TestHelper.AddtoHSCancelBtn.waitForExists(waitingTime);
        Screengrab.screenshot("AddtoHSDialog");
        TestHelper.AddtoHSCancelBtn.click();
    }

    private void takeScreenshotOfTabsTrayAndErase() throws Exception {
        final UiObject mozillaImage = device.findObject(new UiSelector()
                .resourceId("download")
                .enabled(true));

        UiObject imageMenuTitle = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/topPanel")
                .enabled(true));
        UiObject openNewTabTitle = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/design_menu_item_text")
                .text(getString(R.string.mozac_feature_contextmenu_open_link_in_private_tab))
                .enabled(true));
        UiObject multiTabBtn = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/tabs")
                .enabled(true));
        UiObject eraseHistoryBtn = device.findObject(new UiSelector()
                .text(getString(R.string.tabs_tray_action_erase))
                .enabled(true));

        assertTrue(mozillaImage.waitForExists(waitingTime));
        mozillaImage.dragTo(mozillaImage, 7);
        assertTrue(imageMenuTitle.waitForExists(waitingTime));
        assertTrue(imageMenuTitle.exists());
        Screengrab.screenshot("Image_Context_Menu");

        //Open a new tab
        openNewTabTitle.click();
        TestHelper.mDevice.wait(Until.findObject(
                By.res(TestHelper.getAppName(), "snackbar_text")), 5000);
        Screengrab.screenshot("New_Tab_Popup");
        TestHelper.mDevice.wait(Until.gone(
                By.res(TestHelper.getAppName(), "snackbar_text")), 5000);

        assertTrue(multiTabBtn.waitForExists(waitingTime));
        multiTabBtn.click();
        assertTrue(eraseHistoryBtn.waitForExists(waitingTime));
        Screengrab.screenshot("Multi_Tab_Menu");

        eraseHistoryBtn.click();

        device.wait(Until.findObject(
                By.res(TestHelper.getAppName(), "snackbar_text")), waitingTime);

        Screengrab.screenshot("YourBrowsingHistoryHasBeenErased");
    }

    private void takeScreenshotOfFindDialog() throws Exception {
        UiObject findinpageMenuItem = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/find_in_page")
                .enabled(true));
        UiObject findinpageCloseBtn = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/close_find_in_page")
                .enabled(true));

        TestHelper.menuButton.perform(click());
        findinpageMenuItem.waitForExists(waitingTime);
        findinpageMenuItem.click();

        findinpageCloseBtn.waitForExists(waitingTime);
        Screengrab.screenshot("Find_In_Page_Dialog");
        findinpageCloseBtn.click();
    }

    private void takeScreenshotofInsecureCon() throws Exception {

        TestHelper.securityInfoIcon.click();
        TestHelper.identityState.waitForExists(waitingTime);
        Screengrab.screenshot("insecure_connection");
        device.pressBack();
    }

    // This test requires external internet connection
    private void takeScreenshotofSecureCon() throws Exception {

        // take the security info of google.com for https connection
        onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText("www.google.com"), pressImeActionButton());
        TestHelper.waitForWebContent();
        TestHelper.progressBar.waitUntilGone(waitingTime);
        TestHelper.securityInfoIcon.click();
        TestHelper.identityState.waitForExists(waitingTime);
        Screengrab.screenshot("secure_connection");
        device.pressBack();
    }
}
