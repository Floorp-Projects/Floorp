/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.home.HomePager.Page;
import org.mozilla.gecko.R;
import org.mozilla.gecko.tests.helpers.*;
import org.mozilla.gecko.tests.UITestContext;

import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.Solo;

import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.view.View;

/**
 * A class representing any interactions that take place on the Awesomescreen.
 */
public class AboutHomeComponent extends BaseComponent {
    // TODO: Having a specific ordering of pages is prone to fail and thus temporary.
    // Hopefully the work in bug 940565 will alleviate the need for these enums.
    // Explicit ordering of HomePager pages on a phone.
    private enum PhonePage {
        HISTORY,
        TOP_SITES,
        BOOKMARKS,
        READING_LIST
    }

    // Explicit ordering of HomePager pages on a tablet.
    private enum TabletPage {
        TOP_SITES,
        BOOKMARKS,
        READING_LIST,
        HISTORY
    }

    public AboutHomeComponent(final UITestContext testContext) {
        super(testContext);
    }

    private ViewPager getHomePagerView() {
        return (ViewPager) mSolo.getView(R.id.home_pager);
    }

    public AboutHomeComponent assertCurrentPage(final Page expectedPage) {
        assertVisible();

        final int expectedPageIndex = getPageIndexForDevice(expectedPage.ordinal());
        assertEquals("The current HomePager page is " + expectedPage,
                     expectedPageIndex, getHomePagerView().getCurrentItem());
        return this;
    }

    public AboutHomeComponent assertNotVisible() {
        assertFalse("The HomePager is not visible",
                    getHomePagerView().getVisibility() == View.VISIBLE);
        return this;
    }

    public AboutHomeComponent assertVisible() {
        assertEquals("The HomePager is visible",
                     View.VISIBLE, getHomePagerView().getVisibility());
        return this;
    }

    // TODO: Take specific page as parameter rather than swipe in a direction?
    public AboutHomeComponent swipeToPageOnRight() {
        mTestContext.dumpLog("Swiping to the page on the right.");
        swipe(Solo.LEFT);
        return this;
    }

    public AboutHomeComponent swipeToPageOnLeft() {
        mTestContext.dumpLog("Swiping to the page on the left.");
        swipe(Solo.RIGHT);
        return this;
    }

    private void swipe(final int direction) {
        assertVisible();

        final int pageIndex = getHomePagerView().getCurrentItem();
        if (direction == Solo.LEFT) {
            GestureHelper.swipeLeft();
        } else {
            GestureHelper.swipeRight();
        }

        final PagerAdapter adapter = getHomePagerView().getAdapter();
        assertNotNull("The HomePager's PagerAdapter is not null", adapter);

        // Swiping left goes to next, swiping right goes to previous
        final int unboundedPageIndex = pageIndex + (direction == Solo.LEFT ? 1 : -1);
        final int expectedPageIndex = Math.min(Math.max(0, unboundedPageIndex), adapter.getCount() - 1);

        waitForPageIndex(expectedPageIndex);
    }

    private void waitForPageIndex(final int expectedIndex) {
        final String pageName;
        if (DeviceHelper.isTablet()) {
            pageName = TabletPage.values()[expectedIndex].name();
        } else {
            pageName = PhonePage.values()[expectedIndex].name();
        }

        WaitHelper.waitFor("HomePager " + pageName + " page", new Condition() {
            @Override
            public boolean isSatisfied() {
                return (getHomePagerView().getCurrentItem() == expectedIndex);
            }
        });
    }

    /**
     * Gets the page index in the device specific Page enum for the given index in the
     * HomePager.Page enum.
     */
    private int getPageIndexForDevice(final int pageIndex) {
        final String pageName = Page.values()[pageIndex].name();
        final Class devicePageEnum =
                DeviceHelper.isTablet() ? TabletPage.class : PhonePage.class;
        return Enum.valueOf(devicePageEnum, pageName).ordinal();
    }
}
