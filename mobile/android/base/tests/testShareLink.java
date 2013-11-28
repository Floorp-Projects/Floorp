package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

import com.jayway.android.robotium.solo.Condition;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.GridView;
import android.widget.ListView;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.List;

/**
 * This test covers the opening and content of the Share Link pop-up list
 * The test opens the Share menu from the app menu, the URL bar, a link context menu and the Awesomescreen tabs
 */
public class testShareLink extends AboutHomeTest {
    String url;
    String urlTitle = "Big Link";

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testShareLink() {
        url = getAbsoluteUrl("/robocop/robocop_big_link.html");
        ArrayList<String> shareOptions;
        blockForGeckoReady();

        // FIXME: This is a temporary hack workaround for a permissions problem.
        openAboutHomeTab(AboutHomeTabs.READING_LIST);

        inputAndLoadUrl(url);
        verifyPageTitle(urlTitle); // Waiting for page title to ensure the page is loaded

        selectMenuItem("Share");
        if (Build.VERSION.SDK_INT >= 14) {
            // Check for our own sync in the submenu.
            waitForText("Sync$");
        } else {
            waitForText("Share via");
        }

        // Get list of current avaliable share activities and verify them
        shareOptions = getShareOptions();
        ArrayList<String> displayedOptions = getShareOptionsList();
        for (String option:shareOptions) {
             // Verify if the option is present in the list of displayed share options
             mAsserter.ok(optionDisplayed(option, displayedOptions), "Share option found", option);
        }

        // Test share from the urlbar context menu
        mActions.sendSpecialKey(Actions.SpecialKey.BACK); // Close the share menu
        mSolo.clickLongOnText(urlTitle);
        verifySharePopup(shareOptions,"urlbar");

        // The link has a 60px height, so let's try to hit the middle
        float top = mDriver.getGeckoTop() + 30 * mDevice.density;
        float left = mDriver.getGeckoLeft() + mDriver.getGeckoWidth() / 2;
        mSolo.clickLongOnScreen(left, top);
        verifySharePopup("Share Link",shareOptions,"Link");

        // Test the share popup in the Bookmarks page
        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);

        final ListView bookmarksList = findListViewWithTag("bookmarks");
        mAsserter.is(waitForNonEmptyListToLoad(bookmarksList), true, "list is properly loaded");

        int headerViewsCount = bookmarksList.getHeaderViewsCount();
        View bookmarksItem = bookmarksList.getChildAt(headerViewsCount);
        if (bookmarksItem == null) {
            mAsserter.dumpLog("no child at index " + headerViewsCount + "; waiting for one...");
            Condition listWaitCondition = new Condition() {
                @Override
                public boolean isSatisfied() {
                    if (bookmarksList.getChildAt(bookmarksList.getHeaderViewsCount()) == null)
                        return false;
                    return true;
                }
            };
            waitForCondition(listWaitCondition, MAX_WAIT_MS);
            headerViewsCount = bookmarksList.getHeaderViewsCount();
            bookmarksItem = bookmarksList.getChildAt(headerViewsCount);
        }

        mSolo.clickLongOnView(bookmarksItem);
        verifySharePopup(shareOptions,"bookmarks");

        // Prepopulate top sites with history items to overflow tiles.
        // We are trying to move away from using reflection and doing more black-box testing.
        inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_blank_01.html"));
        inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_blank_02.html"));
        inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_blank_03.html"));
        inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_blank_04.html"));
        if (mDevice.type.equals("tablet")) {
            // Tablets have more tile spaces to fill.
            inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_blank_05.html"));
            inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_boxes.html"));
            inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_search.html"));
            inputAndLoadUrl(getAbsoluteUrl("/robocop/robocop_text_page.html"));
        }

        // Test the share popup in Top Sites.
        openAboutHomeTab(AboutHomeTabs.TOP_SITES);

        // Scroll down a bit so that the top sites list has more items on screen.
        int width = mDriver.getGeckoWidth();
        int height = mDriver.getGeckoHeight();
        mActions.drag(width / 2, width / 2, height - 10, height / 2);

        ListView topSitesList = findListViewWithTag("top_sites");
        mAsserter.is(waitForNonEmptyListToLoad(topSitesList), true, "list is properly loaded");
        View mostVisitedItem = topSitesList.getChildAt(topSitesList.getHeaderViewsCount());
        mSolo.clickLongOnView(mostVisitedItem);
        verifySharePopup(shareOptions,"top_sites");

        // Test the share popup in the Most Recent tab
        openAboutHomeTab(AboutHomeTabs.MOST_RECENT);

        ListView mostRecentList = findListViewWithTag("most_recent");
        mAsserter.is(waitForNonEmptyListToLoad(mostRecentList), true, "list is properly loaded");

        // Getting second child after header views because the first is the "Today" label
        View mostRecentItem = mostRecentList.getChildAt(mostRecentList.getHeaderViewsCount() + 1);
        mSolo.clickLongOnView(mostRecentItem);
        verifySharePopup(shareOptions,"most recent");
    }

    public void verifySharePopup(ArrayList<String> shareOptions, String openedFrom) {
        verifySharePopup("Share", shareOptions, openedFrom);
    }

    public void verifySharePopup(String shareItemText, ArrayList<String> shareOptions, String openedFrom) {
        waitForText(shareItemText);
        mSolo.clickOnText(shareItemText);
        waitForText("Share via");
        ArrayList<String> displayedOptions = getSharePopupOption();
        for (String option:shareOptions) {
             // Verify if the option is present in the list of displayed share options
             mAsserter.ok(optionDisplayed(option, displayedOptions), "Share option for " + openedFrom + (openedFrom.equals("urlbar") ? "" : " item") + " found", option);
        }
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
        /**
         * Adding a wait for the page title to make sure the Awesomebar will be dismissed
         * Because of Bug 712370 the Awesomescreen will be dismissed when the Share Menu is closed
         * so there is no need for handeling this different depending on where the share menu was invoced from
         * TODO: Look more into why the delay is needed here now and it was working before
         */
        waitForText(urlTitle);
    }

    // Create a SEND intent and get the possible activities offered
    public ArrayList getShareOptions() {
        ArrayList<String> shareOptions = new ArrayList();
        Activity currentActivity = getActivity();
        final Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.putExtra(Intent.EXTRA_TEXT, url);
        shareIntent.putExtra(Intent.EXTRA_SUBJECT, "Robocop Blank 01");
        shareIntent.setType("text/plain");
        PackageManager pm = currentActivity.getPackageManager();
        List<ResolveInfo> activities = pm.queryIntentActivities(shareIntent, 0);
        for (ResolveInfo activity : activities) {
            shareOptions.add(activity.loadLabel(pm).toString());
        }
        return shareOptions;
    }

    // Traverse the group of views, adding strings from TextViews to the list.
    private void getGroupTextViews(ViewGroup group, ArrayList<String> list) {
        for (int i = 0; i < group.getChildCount(); i++) {
            View child = group.getChildAt(i);
            if (child instanceof AbsListView) {
                getGroupTextViews((AbsListView)child, list);
            } else if (child instanceof ViewGroup) {
                getGroupTextViews((ViewGroup)child, list);
            } else if (child instanceof TextView) {
                String viewText = ((TextView)child).getText().toString();
                if (viewText != null && viewText.length() > 0) {
                    list.add(viewText);
                }
            }
        }
    }

    // Traverse the group of views, adding strings from TextViews to the list.
    // This override is for AbsListView, which has adapters. If adapters are
    // available, it is better to use them so that child views that are not
    // yet displayed can be examined.
    private void getGroupTextViews(AbsListView group, ArrayList<String> list) {
        for (int i = 0; i < group.getAdapter().getCount(); i++) {
            View child = group.getAdapter().getView(i, null, group);
            if (child instanceof AbsListView) {
                getGroupTextViews((AbsListView)child, list);
            } else if (child instanceof ViewGroup) {
                getGroupTextViews((ViewGroup)child, list);
            } else if (child instanceof TextView) {
                String viewText = ((TextView)child).getText().toString();
                if (viewText != null && viewText.length() > 0) {
                    list.add(viewText);
                }
            }
        }
    }

    public ArrayList<String> getSharePopupOption() {
        ArrayList<String> displayedOptions = new ArrayList();
        AbsListView shareMenu = getDisplayedShareList();
        getGroupTextViews(shareMenu, displayedOptions);
        return displayedOptions;
    }

    public ArrayList<String> getShareSubMenuOption() {
        ArrayList<String> displayedOptions = new ArrayList();
        AbsListView shareMenu = getDisplayedShareList();
        getGroupTextViews(shareMenu, displayedOptions);
        return displayedOptions;
    }

    public ArrayList<String> getShareOptionsList() {
        if (Build.VERSION.SDK_INT >= 14) {
            return getShareSubMenuOption();
        } else {
            return getSharePopupOption();
        }
    }

    private boolean optionDisplayed(String shareOption, ArrayList<String> displayedOptions) {
        for (String displayedOption: displayedOptions) {
            if (shareOption.equals(displayedOption)) {
                return true;
            }
        }
        return false;
    }

    private AbsListView mViewGroup;

    private AbsListView getDisplayedShareList() {
        mViewGroup = null;
        boolean success = waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                ArrayList<View> views = mSolo.getCurrentViews();
                for (View view : views) {
                    // List may be displayed in different view formats. 
                    // On JB, GridView is common; on ICS-, ListView is common.
                    if (view instanceof ListView ||
                        view instanceof GridView) {
                        mViewGroup = (AbsListView)view;
                        return true;
                    }
                }
                return false;
            }
        }, MAX_WAIT_MS);
        mAsserter.ok(success,"Got the displayed share options?", "Got the share options view");
        return mViewGroup;
    }
}
