/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.os.SystemClock;
import android.support.test.runner.AndroidJUnit4;

import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
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
import static android.support.test.espresso.matcher.ViewMatchers.withText;

@RunWith(AndroidJUnit4.class)
public class HomeScreenScreenshots extends ScreenshotTest {

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    private enum TipTypes {
        TIP_CREATETP (1),
        TIP_CREATEHS (2),
        TIP_CREATEDEFAULTBROWSER (3),
        TIP_AC_URL (4),
        TIP_NEWTAB (5),
        TIP_REQDESKTOP (6);
        private int value;

        TipTypes(int value) {
            this.value = value;
        }
    }

    @Test
    public void takeScreenshotOfHomeScreen() {
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()));
        SystemClock.sleep(5000);
        Screengrab.screenshot("Home_View");
    }

    @Test
    public void takeScreenshotOfMenu() {
        TestHelper.menuButton.perform(click());

        onView(withText(R.string.menu_whats_new))
                .check(matches(isDisplayed()));

        Screengrab.screenshot("MainViewMenu");
    }

    @Test
    public void takeScreenshotOfTips() {
        for (TipTypes tip: TipTypes.values()) {
            onView(withId(R.id.urlView))
                    .check(matches(isDisplayed()))
                    .check(matches(hasFocus()))
                    .perform(click(), replaceText("l10n:tip:" + tip.value), pressImeActionButton());

            onView(withId(R.id.homeViewTipsLabel))
                    .check(matches(isDisplayed()));
            Screengrab.screenshot("MainViewTip_" + tip.name());
        }
    }
}
