/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.os.SystemClock;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule;
import org.mozilla.focus.helpers.TestHelper;
import org.mozilla.focus.utils.AppConstants;

import tools.fastlane.screengrab.Screengrab;
import tools.fastlane.screengrab.locale.LocaleTestRule;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static junit.framework.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public class FirstRunScreenshots extends ScreenshotTest {
    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new MainActivityFirstrunTestRule(true) {
        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            // This test is for webview only for now.
            org.junit.Assume.assumeTrue(!AppConstants.INSTANCE.isGeckoBuild() &&
                    !AppConstants.INSTANCE.isKlarBuild());
        }
    };

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    @Test
    public void takeScreenshotsOfFirstrun() throws UiObjectNotFoundException, InterruptedException {
        onView(withText(R.string.firstrun_defaultbrowser_title))
                .check(matches(isDisplayed()));

        device.waitForIdle();
        SystemClock.sleep(5000);

        Screengrab.screenshot("Onboarding_1_View");
        TestHelper.nextBtn.click();

        assertTrue(device.findObject(new UiSelector()
                .text(getString(R.string.firstrun_search_title))
                .enabled(true)
        ).waitForExists(waitingTime));

        Screengrab.screenshot("Onboarding_2_View");
        TestHelper.nextBtn.click();

        assertTrue(device.findObject(new UiSelector()
                .text(getString(R.string.firstrun_shortcut_title))
                .enabled(true)
        ).waitForExists(waitingTime));

        Screengrab.screenshot("Onboarding_3_View");
        TestHelper.nextBtn.click();

        assertTrue(device.findObject(new UiSelector()
                .text(getString(R.string.firstrun_privacy_title))
                .enabled(true)
        ).waitForExists(waitingTime));

        Screengrab.screenshot("Onboarding_last_View");
    }
}
