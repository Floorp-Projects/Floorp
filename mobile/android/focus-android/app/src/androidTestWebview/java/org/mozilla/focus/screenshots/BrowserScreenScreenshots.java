/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import tools.fastlane.screengrab.Screengrab;
import tools.fastlane.screengrab.locale.LocaleTestRule;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.pressImeActionButton;
import static android.support.test.espresso.action.ViewActions.replaceText;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasFocus;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.containsString;

@RunWith(AndroidJUnit4.class)
public class BrowserScreenScreenshots extends ScreenshotTest {
    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new MainActivityFirstrunTestRule(false);

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
        takeScreenshotsOfBrowsingView();
        takeScreenshotsOfMenu();
        takeScreenshotsOfOpenWithAndShare();
        takeAddToHomeScreenScreenshot();
        takeScreenshotOfTabsTrayAndErase();
    }

    private void takeScreenshotsOfBrowsingView() {
        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));
        Screengrab.screenshot("LocationBarEmptyState");

        /* Autocomplete View */
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText("mozilla"));

        assertTrue(TestHelper.hint.waitForExists(waitingTime));

        Screengrab.screenshot("SearchFor");

        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText(webServer.url("/").toString()), pressImeActionButton());

        device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/webview")
                .enabled(true))
                .waitForExists(waitingTime);

        onView(withId(R.id.display_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(containsString(webServer.getHostName()))));

        Screengrab.screenshot("BrowserView");
    }

    private void takeScreenshotsOfMenu() {
        TestHelper.menuButton.perform(click());
        Screengrab.screenshot("BrowserViewMenu");
    }

    private void takeScreenshotsOfOpenWithAndShare() throws Exception {
        /* Open_With View */
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

        assertTrue(TestHelper.AddtoHSmenuItem.waitForExists(waitingTime));
        TestHelper.AddtoHSmenuItem.click();

        assertTrue(TestHelper.AddtoHSCancelBtn.waitForExists(waitingTime));
        Screengrab.screenshot("AddtoHSDialog");

        TestHelper.AddtoHSCancelBtn.click();
        Assert.assertTrue(TestHelper.browserURLbar.waitForExists(waitingTime));
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
                .index(0)
                .enabled(true));
        UiObject multiTabBtn = device.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/tabs")
                .enabled(true));
        UiObject eraseHistoryBtn = device.findObject(new UiSelector()
                .text(getString(R.string.tabs_tray_action_erase))
                .enabled(true));

        Assert.assertTrue(mozillaImage.waitForExists(waitingTime));
        mozillaImage.dragTo(mozillaImage, 7);
        assertTrue(imageMenuTitle.waitForExists(waitingTime));
        Assert.assertTrue(imageMenuTitle.exists());
        Screengrab.screenshot("Image_Context_Menu");

        //Open a new tab
        openNewTabTitle.click();

        assertTrue(multiTabBtn.waitForExists(waitingTime));
        multiTabBtn.click();
        assertTrue(eraseHistoryBtn.waitForExists(waitingTime));
        Screengrab.screenshot("Multi_Tab_Menu");

        eraseHistoryBtn.click();

        device.wait(Until.findObject(By.res(TestHelper.getAppName(), "snackbar_text")), waitingTime);

        Screengrab.screenshot("YourBrowsingHistoryHasBeenErased");
    }
}
