/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.os.SystemClock;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;

import org.junit.ClassRule;
import org.junit.Ignore;
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

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static junit.framework.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
public class FirstRunScreenshots extends ScreenshotTest {
    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new MainActivityFirstrunTestRule(true) {
        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();
        }
    };

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    @Test
    public void takeScreenshotsOfFirstrun() throws UiObjectNotFoundException {
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
