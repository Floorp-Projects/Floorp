/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.os.Build;
import android.support.test.espresso.web.webdriver.Locator;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiSelector;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule;
import org.mozilla.focus.helpers.TestHelper;

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
import static android.support.test.espresso.web.sugar.Web.onWebView;
import static android.support.test.espresso.web.webdriver.DriverAtoms.findElement;
import static android.support.test.espresso.web.webdriver.DriverAtoms.webClick;
import static android.support.test.espresso.web.webdriver.DriverAtoms.webScrollIntoView;
import static junit.framework.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public class ErrorPagesScreenshots extends ScreenshotTest {
    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new MainActivityFirstrunTestRule(false);

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

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

    @Test
    public void takeScreenshotsOfErrorPages() throws Exception {
        for (ErrorTypes error: ErrorTypes.values()) {
            onView(withId(R.id.urlView))
                    .check(matches(isDisplayed()))
                    .check(matches(hasFocus()))
                    .perform(click(), replaceText("error:" + error.value), pressImeActionButton());

            assertTrue(TestHelper.webView.waitForExists(waitingTime));
            assertTrue(TestHelper.progressBar.waitUntilGone(waitingTime));

            // Android O has an issue with using Locator.ID
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                UiObject tryAgainBtn = device.findObject(new UiSelector()
                        .descriptionContains(getString(R.string.errorpage_refresh))
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

            onView(withId(R.id.display_url))
                    .check(matches(isDisplayed()))
                    .perform(click());
        }
    }
}
