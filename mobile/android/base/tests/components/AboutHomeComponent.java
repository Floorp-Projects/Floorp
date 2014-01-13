/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.Actions;
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
    private static final String LOGTAG = AboutHomeComponent.class.getSimpleName();

    // The different types of panels that can be present on about:home
    public enum PanelType {
        HISTORY,
        TOP_SITES,
        BOOKMARKS,
        READING_LIST
    }

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

    // The percentage of the page to swipe between 0 and 1. This value was set through
    // testing: 0.55f was tested on try and fails on armv6 devices.
    private static final float SWIPE_PERCENTAGE = 0.70f;

    public AboutHomeComponent(final UITestContext testContext) {
        super(testContext);
    }

    private ViewPager getHomePagerView() {
        return (ViewPager) mSolo.getView(R.id.home_pager);
    }

    public AboutHomeComponent assertCurrentPage(final PanelType expectedPage) {
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

    public AboutHomeComponent swipeToPageOnRight() {
        mTestContext.dumpLog(LOGTAG, "Swiping to the page on the right.");
        swipeToPage(Solo.RIGHT);
        return this;
    }

    public AboutHomeComponent swipeToPageOnLeft() {
        mTestContext.dumpLog(LOGTAG, "Swiping to the page on the left.");
        swipeToPage(Solo.LEFT);
        return this;
    }

    private void swipeToPage(final int pageDirection) {
        assertTrue("Swiping in a vaild direction",
                pageDirection == Solo.LEFT || pageDirection == Solo.RIGHT);
        assertVisible();

        final int pageIndex = getHomePagerView().getCurrentItem();

        mSolo.scrollViewToSide(getHomePagerView(), pageDirection, SWIPE_PERCENTAGE);

        // The page on the left is a lower index and vice versa.
        final int unboundedPageIndex = pageIndex + (pageDirection == Solo.LEFT ? -1 : 1);
        final int pageCount = DeviceHelper.isTablet() ?
                TabletPage.values().length : PhonePage.values().length;
        final int maxPageIndex = pageCount - 1;
        final int expectedPageIndex = Math.min(Math.max(0, unboundedPageIndex), maxPageIndex);

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
     * PanelType enum.
     */
    private int getPageIndexForDevice(final int pageIndex) {
        final String pageName = PanelType.values()[pageIndex].name();
        final Class devicePageEnum =
                DeviceHelper.isTablet() ? TabletPage.class : PhonePage.class;
        return Enum.valueOf(devicePageEnum, pageName).ordinal();
    }
}
