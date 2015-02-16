/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.ArrayList;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.home.HomePager;

import android.support.v4.view.ViewPager;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.TabWidget;
import android.widget.TextView;

import com.jayway.android.robotium.solo.Condition;

/**
 * This class is an extension of BaseTest that helps with interaction with about:home
 * This class contains methods that access the different tabs from about:home, methods that get information like history and bookmarks from the database, edit and remove bookmarks and history items
 * The purpose of this class is to collect all the logically connected methods that deal with about:home
 * To use any of these methods in your test make sure it extends AboutHomeTest instead of BaseTest
 */
abstract class AboutHomeTest extends PixelTest {
    protected enum AboutHomeTabs {
        RECENT_TABS,
        HISTORY,
        TOP_SITES,
        BOOKMARKS,
        READING_LIST
    };

    private final ArrayList<String> aboutHomeTabs = new ArrayList<String>() {{
                  add("TOP_SITES");
                  add("BOOKMARKS");
                  add("READING_LIST");
              }};


    @Override
    public void setUp() throws Exception {
        super.setUp();

        if (aboutHomeTabs.size() < 4) {
            // Update it for tablets vs. phones.
            if (mDevice.type.equals("phone")) {
                aboutHomeTabs.add(0, AboutHomeTabs.HISTORY.toString());
                aboutHomeTabs.add(0, AboutHomeTabs.RECENT_TABS.toString());
            } else {
                aboutHomeTabs.add(AboutHomeTabs.HISTORY.toString());
                aboutHomeTabs.add(AboutHomeTabs.RECENT_TABS.toString());
            }
        }
    }

    /**
     * FIXME: Write new versions of these methods and update their consumers to use the new about:home pages.
     */
    protected ListView getHistoryList(String waitText, int expectedChildCount) {
        return null;
    }
    protected ListView getHistoryList(String waitText) {
        return null;
    }

    // Returns true if the bookmark is displayed in the bookmarks tab, false otherwise - does not check in folders
    protected void isBookmarkDisplayed(final String url) {
        boolean isCorrect = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                View bookmark = getDisplayedBookmark(url);
                return bookmark != null;
            }
        }, MAX_WAIT_MS);

        mAsserter.ok(isCorrect, "Checking that " + url + " displayed as a bookmark", url + " displayed");
    }

    // Loads a bookmark by tapping on the bookmark view in the Bookmarks tab
    protected void loadBookmark(String url) {
        View bookmark = getDisplayedBookmark(url);
        if (bookmark != null) {
            Actions.EventExpecter contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");
            mSolo.clickOnView(bookmark);
            contentEventExpecter.blockForEvent();
            contentEventExpecter.unregisterListener();
        } else {
            mAsserter.ok(false, url + " is not one of the displayed bookmarks", "Please make sure the url provided is bookmarked");
        }
    }

    // Opens the bookmark context menu by long-tapping on it
    protected void openBookmarkContextMenu(String url) {
        View bookmark = getDisplayedBookmark(url);
        if (bookmark != null) {
            mSolo.waitForView(bookmark);
            mSolo.clickLongOnView(bookmark, LONG_PRESS_TIME);
            mSolo.waitForDialogToOpen();
        } else {
            mAsserter.ok(false, url + " is not one of the displayed bookmarks", "Please make sure the url provided is bookmarked");
        }
    }

    // @return the View associated with bookmark for the provided url or null if the link is not bookmarked
    protected View getDisplayedBookmark(String url) {
        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);
        mSolo.hideSoftKeyboard();
        getInstrumentation().waitForIdleSync();
        ListView bookmarksTabList = findListViewWithTag(HomePager.LIST_TAG_BOOKMARKS);
        waitForNonEmptyListToLoad(bookmarksTabList);
        ListAdapter adapter = bookmarksTabList.getAdapter();
        if (adapter != null) {
            for (int i = 0; i < adapter.getCount(); i++ ) {
                // I am unable to click the view taken with getView for some reason so getting the child at i
                bookmarksTabList.smoothScrollToPosition(i);
                View bookmarkView = bookmarksTabList.getChildAt(i);
                if (bookmarkView instanceof android.widget.LinearLayout) {
                    ViewGroup bookmarkItemView = (ViewGroup) bookmarkView;
                    for (int j = 0 ; j < bookmarkItemView.getChildCount(); j++) {
                         View bookmarkContent = bookmarkItemView.getChildAt(j);
                         if (bookmarkContent instanceof android.widget.LinearLayout) {
                             ViewGroup bookmarkItemLayout = (ViewGroup) bookmarkContent;
                             for (int k = 0 ; k < bookmarkItemLayout.getChildCount(); k++) {
                                  // Both the title and url are represented as text views so we can cast the view without any issues
                                  TextView bookmarkTextContent = (TextView)bookmarkItemLayout.getChildAt(k);
                                  if (url.equals(bookmarkTextContent.getText().toString())) {
                                      return bookmarkView;
                                  }
                            }
                        }
                    }
                }
            }
        }
        return null;
    }

    /**
     * Waits for the given ListView to have a non-empty adapter and be populated
     * with a minimum number of items.
     *
     * This method will return false if the given ListView or its adapter is null,
     * or if the ListView does not have the minimum number of items.
     */
    protected boolean waitForListToLoad(final ListView listView, final int minSize) {
        Condition listWaitCondition = new Condition() {
            @Override
            public boolean isSatisfied() {
                if (listView == null) {
                    return false;
                }

                final ListAdapter adapter = listView.getAdapter();
                if (adapter == null) {
                    return false;
                }

                return (listView.getCount() - listView.getHeaderViewsCount() >= minSize);
            }
        };
        return waitForCondition(listWaitCondition, MAX_WAIT_MS);
    }

    protected boolean waitForNonEmptyListToLoad(final ListView listView) {
        return waitForListToLoad(listView, 1);
    }

    /**
     * Get an active ListView with the specified tag .
     *
     * This method uses the predefined tags in HomePager.
     */
    protected final ListView findListViewWithTag(String tag) {
        for (ListView listView : mSolo.getCurrentViews(ListView.class)) {
            final String listTag = (String) listView.getTag();
            if (TextUtils.isEmpty(listTag)) {
                continue;
            }

            if (TextUtils.equals(listTag, tag)) {
                return listView;
            }
        }

        return null;
    }

    // A wait in order for the about:home tab to be rendered after drag/tab selection
    private void waitForAboutHomeTab(final int tabIndex) {
        boolean correctTab = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                ViewPager pager = mSolo.getView(ViewPager.class, 0);
                return (pager.getCurrentItem() == tabIndex);
            }
        }, MAX_WAIT_MS);
        mAsserter.ok(correctTab, "Checking that the correct tab is displayed", "The " + aboutHomeTabs.get(tabIndex) + " tab is displayed");
    }

    private void clickAboutHomeTab(AboutHomeTabs tab) {
        mSolo.clickOnText(tab.toString().replace("_", " "));
    }

    /**
     * Swipes to an about:home tab.
     * @param int swipeVector Value and direction to swipe (go left for negative, right for positive).
     */
    private void swipeAboutHome(int swipeVector) {
        // Increase swipe width, which will especially impact tablets.
        int swipeWidth = mDriver.getGeckoWidth() - 1;
        int swipeHeight = mDriver.getGeckoHeight() / 2;

        if (swipeVector >= 0) {
            // Emulate swipe motion from right to left.
            for (int i = 0; i < swipeVector; i++) {
                mActions.drag(swipeWidth, 0, swipeHeight, swipeHeight);
                mSolo.sleep(100);
            }
        } else {
            // Emulate swipe motion from left to right.
            for (int i = 0; i > swipeVector; i--) {
                mActions.drag(0, swipeWidth, swipeHeight, swipeHeight);
                mSolo.sleep(100);
            }
        }
    }

    /**
     * This method can be used to open the different tabs of about:home.
     *
     * @param AboutHomeTabs enum item
     */
    protected void openAboutHomeTab(AboutHomeTabs tab) {
        focusUrlBar();
        ViewPager pager = mSolo.getView(ViewPager.class, 0);
        final int currentTabIndex = pager.getCurrentItem();
        int tabOffset;

        // Handle tablets by just clicking the visible tab title.
        if (mDevice.type.equals("tablet")) {
            clickAboutHomeTab(tab);
            return;
        }

        // Handle phones (non-tablets).
        tabOffset = aboutHomeTabs.indexOf(tab.toString()) - currentTabIndex;
        swipeAboutHome(tabOffset);
        waitForAboutHomeTab(aboutHomeTabs.indexOf(tab.toString()));
    }
}
