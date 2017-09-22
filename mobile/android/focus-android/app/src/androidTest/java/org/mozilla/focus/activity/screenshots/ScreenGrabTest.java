package org.mozilla.focus.activity.screenshots;

import android.app.Instrumentation;
import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.web.webdriver.Locator;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.text.format.DateUtils;

import junit.framework.Assert;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.activity.TestHelper;
import org.mozilla.focus.activity.helpers.HostScreencapScreenshotStrategy;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

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
import static android.support.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static android.support.test.espresso.web.sugar.Web.onWebView;
import static android.support.test.espresso.web.webdriver.DriverAtoms.findElement;
import static android.support.test.espresso.web.webdriver.DriverAtoms.getText;
import static android.support.test.espresso.web.webdriver.DriverAtoms.webClick;
import static android.support.test.espresso.web.webdriver.DriverAtoms.webScrollIntoView;
import static android.view.KeyEvent.KEYCODE_ENTER;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.equalTo;
import static org.mozilla.focus.activity.TestHelper.browserURLbar;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

@RunWith(AndroidJUnit4.class)
public class ScreenGrabTest {
    private static final long waitingTime = DateUtils.SECOND_IN_MILLIS * 10;
    private static final long loadingWaitingTime = DateUtils.SECOND_IN_MILLIS * 20;
    private static final String TEST_PATH = "/";

    private enum ErrorTypes {
        ERROR_UNKNOWN (-1),
        ERROR_HOST_LOOKUP (-2),
        ERROR_CONNECT (-6),
        ERROR_TIMEOUT (-8),
        ERROR_REDIRECT_LOOP (-9),
        ERROR_UNSUPPORTED_SCHEME (-10),
        ERROR_FAILED_SSL_HANDSHAKE (-11),
        ERROR_BAD_URL (-12),
        ERROR_TOO_MANY_REQUESTS (-15);
        private int value;

        ErrorTypes(int value) {
            this.value = value;
        }
    }

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    private MockWebServer webServer;

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new ActivityTestRule<MainActivity>(MainActivity.class) {

        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            Context appContext = InstrumentationRegistry.getInstrumentation()
                    .getTargetContext()
                    .getApplicationContext();
            Resources resources = appContext.getResources();

            PreferenceManager.getDefaultSharedPreferences(appContext)
                    .edit()
                    .putBoolean(FIRSTRUN_PREF, false)
                    .putBoolean(resources.getString(R.string.pref_key_secure), false)
                    .apply();

            webServer = new MockWebServer();

            try {
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("plain_test.html")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("image_test.html")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("rabbit.jpg")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("download.jpg")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("download.jpg")));

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

    @Test
    public void screenGrabTest() throws InterruptedException, UiObjectNotFoundException {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final Context context = instrumentation.getTargetContext();
        final UiDevice device = UiDevice.getInstance(instrumentation);

        // Use this to switch between default strategy and HostScreencap strategy
        //Screengrab.setDefaultScreenshotStrategy(new UiAutomatorScreenshotStrategy());
        Screengrab.setDefaultScreenshotStrategy(new HostScreencapScreenshotStrategy(device));

        takeScreenshotsOfFirstrun(context, device);

        takeScreenshotOfHomeScreen();
        takeScreenshotOfMenuAndYourRightsPage(device);
        takeScreenshotOfAboutPage(context, device);

        takeScreenshotOfUrlBarAndBrowserView(device);
        takeScreenshotOfOpenWithAndShareViews(device);
        takeAddToHomeScreenScreenshot();
        takeScreenshotOfNotification(context, device);
        takeScreenshotOfEraseSnackbar(device);

        takeScreenshotOfSettings(device);

        // Temporarily disabled: Our emulator image doesn't include Google Play - So we can't take
        // a screenshot of this dialog.
        // takeScreenshotOfGooglePlayDialog(device);

        takeScreenshotOfContextMenu(context, device);
        takeScreenshotOfErrorPages(context, device);
    }

    private void takeScreenshotsOfFirstrun(Context context, UiDevice device) throws UiObjectNotFoundException {
        /* Wait for app to load, and take the First View screenshot */

        assertTrue(device.findObject(new UiSelector()
                .text(context.getString(R.string.firstrun_defaultbrowser_title))
                .enabled(true)
        ).waitForExists(waitingTime));

        Screengrab.screenshot("Onboarding_1_View");
        TestHelper.nextBtn.click();

        assertTrue(device.findObject(new UiSelector()
                .text(context.getString(R.string.firstrun_search_title))
                .enabled(true)
        ).waitForExists(waitingTime));

        Screengrab.screenshot("Onboarding_2_View");
        TestHelper.nextBtn.click();

        assertTrue(device.findObject(new UiSelector()
                .text(context.getString(R.string.firstrun_shortcut_title))
                .enabled(true)
        ).waitForExists(waitingTime));

        Screengrab.screenshot("Onboarding_3_View");
        TestHelper.nextBtn.click();

        assertTrue(device.findObject(new UiSelector()
                .text(context.getString(R.string.firstrun_privacy_title))
                .enabled(true)
        ).waitForExists(waitingTime));

        Screengrab.screenshot("Onboarding_last_View");
        TestHelper.finishBtn.click();
    }

    private void takeScreenshotOfHomeScreen() {
        /* Home View*/
        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));
        Screengrab.screenshot("Home_View");
    }

    private void takeScreenshotOfMenuAndYourRightsPage(UiDevice device) throws UiObjectNotFoundException {
        TestHelper.menuButton.perform(click());

        assertTrue(TestHelper.RightsItem.waitForExists(waitingTime));
        Screengrab.screenshot("MainViewMenu");

        TestHelper.RightsItem.click();
       assertTrue(TestHelper.webView.waitForExists(waitingTime));
        onWebView()
                .withElement(findElement(Locator.ID, "first"))
                .perform(webClick());

        Screengrab.screenshot("YourRights_Page");

        device.pressBack();
    }

    private void takeScreenshotOfAboutPage(Context context, UiDevice device) throws UiObjectNotFoundException {
        TestHelper.menuButton.perform(click());
        TestHelper.AboutItem.click();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
        onWebView()
                .withElement(findElement(Locator.ID, "wordmark"))
                .perform(webClick());

        Screengrab.screenshot("About_Page");
        device.pressBack();
    }

    private void takeScreenshotOfUrlBarAndBrowserView(UiDevice device) throws UiObjectNotFoundException {
        /* Location Bar View */
        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));
        Screengrab.screenshot("LocationBarEmptyState");

        /* Autocomplete View */
        onView(withId(R.id.url_edit))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText("mozilla"));

        assertTrue(TestHelper.hint.waitForExists(waitingTime));
        Screengrab.screenshot("SearchFor");

        onView(withId(R.id.url_edit))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(click(), replaceText(webServer.url(TEST_PATH).toString()), pressImeActionButton());

        device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/webview")
                .enabled(true))
                .waitForExists(waitingTime);

        onWebView()
                .withElement(findElement(Locator.ID, "header"))
                .check(webMatches(getText(), equalTo("focus test page")));

        TestHelper.menuButton.perform(click());
        Screengrab.screenshot("BrowserViewMenu");
    }

    private void takeScreenshotOfOpenWithAndShareViews(UiDevice device) throws UiObjectNotFoundException {
        /* Open_With View */
        UiObject openWithBtn = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/open_select_browser")
                .enabled(true));
        assertTrue(openWithBtn.waitForExists(waitingTime));
        openWithBtn.click();
        UiObject shareList = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/apps")
                .enabled(true));
        assertTrue(shareList.waitForExists(waitingTime));
        Screengrab.screenshot("OpenWith_Dialog");

        /* Share View */
        UiObject shareBtn = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/share")
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

    private void takeScreenshotOfNotification(Context context, UiDevice device) throws UiObjectNotFoundException {
        device.openNotification();

        final UiObject eraseNotification = device.findObject(new UiSelector()
                .descriptionContains(context.getString(R.string.notification_erase_text))
                .resourceId("android:id/text")
                .enabled(true));

        final UiObject openAction = device.findObject(new UiSelector()
                .descriptionContains(context.getString(R.string.notification_action_open))
                .resourceId("android:id/action0")
                .enabled(true));

        if (!openAction.waitForExists(waitingTime)) {
            // The notification is not expanded. Let's expand it now.
            assertTrue(eraseNotification.exists());
            TestHelper.notificationExpandSwitch.click();
            assertTrue(openAction.waitForExists(waitingTime));
        }
        Screengrab.screenshot("DeleteHistory_NotificationBar");
        device.pressBack();
    }

    private void takeScreenshotOfEraseSnackbar(UiDevice device) throws UiObjectNotFoundException {
        final UiObject floatingEraseButton = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/erase")
                .enabled(true));

        assertTrue(floatingEraseButton.waitForExists(waitingTime));

        floatingEraseButton.click();

        device.wait(Until.findObject(By.res("org.mozilla.focus.debug","snackbar_text")), waitingTime);
        Screengrab.screenshot("YourBrowsingHistoryHasBeenErased");
    }

    private void takeScreenshotOfSettings(UiDevice device) throws UiObjectNotFoundException {
        /* Take Settings View */
        TestHelper.menuButton.perform(click());
        TestHelper.settingsMenuItem.click();
        assertTrue(TestHelper.settingsHeading.waitForExists(waitingTime));
        Screengrab.screenshot("Settings_View_Top");

        /* Language List (First page only */
        UiObject LanguageSelection = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(0));
        LanguageSelection.click();
        TestHelper.settingsHeading.waitUntilGone(waitingTime);

        UiObject CancelBtn =  device.findObject(new UiSelector()
                .resourceId("android:id/button2")
                .enabled(true));
        Screengrab.screenshot("Language_Selection");
        CancelBtn.click();
        assertTrue(TestHelper.settingsHeading.waitForExists(waitingTime));

        /* Search Engine List */
        UiObject SearchEngineSelection = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(1));
        SearchEngineSelection.click();
        TestHelper.settingsHeading.waitUntilGone(waitingTime);
        UiObject SearchEngineList = new UiScrollable(new UiSelector()
                .resourceId("android:id/select_dialog_listview").enabled(true));
        UiObject FirstSelection = SearchEngineList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(0));
        Screengrab.screenshot("SearchEngine_Selection");

        /* scroll down */
        FirstSelection.click();
        assertTrue(TestHelper.settingsHeading.waitForExists(waitingTime));
        UiScrollable settingsView = new UiScrollable(new UiSelector().scrollable(true));
        settingsView.scrollToEnd(4);
        Screengrab.screenshot("Settings_View_Bottom");

        device.pressBack();
    }

    private void takeScreenshotOfContextMenu(Context context, UiDevice device) throws UiObjectNotFoundException {
                /* Take image context menu screenshot */
        UiObject titleMsg = device.findObject(new UiSelector()
                .description("focus test page")
                .enabled(true));
        UiObject mozillaImage = device.findObject(new UiSelector()
                .description("download icon")
                .enabled(true));
        UiObject imageMenuTitle = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/topPanel")
                .enabled(true));
        UiObject openNewTabTitle = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/design_menu_item_text")
                .index(0)
                .enabled(true));
        UiObject multiTabBtn = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/tabs")
                .enabled(true));

        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.pressEnterKey();

        assertTrue(TestHelper.webView.waitForExists(waitingTime));

        onWebView()
                .withTimeout(loadingWaitingTime, TimeUnit.MILLISECONDS)
                .withElement(findElement(Locator.ID, "header"))
                .check(webMatches(getText(), equalTo("focus test page")));

        assertTrue(titleMsg.waitForExists(waitingTime));
        assertTrue("Website title loaded", titleMsg.exists());
        Assert.assertTrue(mozillaImage.exists());
        mozillaImage.dragTo(mozillaImage,7);
        assertTrue(imageMenuTitle.waitForExists(waitingTime));
        Assert.assertTrue(imageMenuTitle.exists());
        Screengrab.screenshot("Image_Context_Menu");

        //Open a new tab
        openNewTabTitle.click();
        assertTrue(multiTabBtn.waitForExists(waitingTime));
        multiTabBtn.click();
        Screengrab.screenshot("Multi_Tab_Menu");
        TestHelper.pressBackKey();
        device.openNotification();

        final UiObject notificationErase = device.findObject(new UiSelector()
                .text(context.getString(R.string.notification_erase_text))
                .resourceId("android:id/text")
                .enabled(true));

        assertTrue(notificationErase.waitForExists(waitingTime));
        notificationErase.click();
    }

    private void takeScreenshotOfGooglePlayDialog(UiDevice device) throws UiObjectNotFoundException {
        final String marketURL = "market://details?id=org.mozilla.firefox&referrer=utm_source%3D" +
                "mozilla%26utm_medium%3DReferral%26utm_campaign%3Dmozilla-org";

        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));
        TestHelper.inlineAutocompleteEditText.setText(marketURL);
        device.pressKeyCode(KEYCODE_ENTER);

        UiObject cancelBtn = device.findObject(new UiSelector()
                .resourceId("android:id/button2"));
        UiObject alert = device.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/alertTitle"));

        assertTrue(alert.waitForExists(waitingTime));
        assertTrue(cancelBtn.waitForExists(waitingTime));
        Screengrab.screenshot("Redirect_Outside");
        cancelBtn.click();

        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));
        device.pressBack();
    }

    private void takeScreenshotOfErrorPages(Context context, UiDevice device) throws UiObjectNotFoundException {
        for (ScreenGrabTest.ErrorTypes error: ScreenGrabTest.ErrorTypes.values()) {
            assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));

            TestHelper.inlineAutocompleteEditText.setText("error:"+ error.value);
            device.pressKeyCode(KEYCODE_ENTER);

            assertTrue(TestHelper.webView.waitForExists(waitingTime));

            // Android O has an issue with using Locator.ID
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                UiObject tryAgainBtn = device.findObject(new UiSelector()
                        .descriptionContains(context.getString(R.string.errorpage_refresh))
                        .clickable(true));
                assertTrue(tryAgainBtn.waitForExists(waitingTime));
            } else {
                onWebView()
                        .withElement(findElement(Locator.ID, "errorTitle"))
                        .perform(webClick());

                onWebView()
                        .withElement(findElement(Locator.ID, "errorTryAgain"))
                        .perform(webScrollIntoView());
            }
            Screengrab.screenshot(error.name());
            browserURLbar.click();
        }
    }
}
