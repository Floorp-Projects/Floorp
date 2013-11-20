package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;
import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.Solo;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import java.util.ArrayList;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * This patch tests the Reader Mode feature by adding and removing items in reading list
 * checks the reader toolbar functionality(share, add/remove to reading list, go to reading list)
 * accessing a page from reading list menu, checks that the reader icon is associated in History tab
 * and that the reading list is properly populated after adding or removing reader items
 */
public class testReaderMode extends AboutHomeTest {
    static final int EVENT_CLEAR_DELAY_MS = 3000;
    static final int READER_ICON_MAX_WAIT_MS = 15000;

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }
    public void testReaderMode() {
        blockForGeckoReady();

        Actions.EventExpecter contentEventExpecter;
        Actions.EventExpecter contentReaderAddedExpecter;
        Actions.EventExpecter faviconExpecter;
        Actions.EventExpecter contentPageShowExpecter;
        Actions.RepeatedEventExpecter paintExpecter;
        ListView list;
        View child;
        View readerIcon;
        String textUrl = getAbsoluteUrl(StringHelper.ROBOCOP_TEXT_PAGE_URL);
        String devType = mDevice.type;
        int childNo;
        int height;
        int width;

        loadAndPaint(textUrl);

        // Add the page to the Reading List using long click on the reader icon
        readerIcon = getReaderIcon();
        contentReaderAddedExpecter = mActions.expectGeckoEvent("Reader:Added");
        mSolo.clickLongOnView(readerIcon);
        String eventData = contentReaderAddedExpecter.blockForEventData();
        isAdded(eventData);
        contentReaderAddedExpecter.unregisterListener();

        // Try to add the page to the Reading List using long click on the reader icon a second time
        readerIcon = getReaderIcon();
        contentReaderAddedExpecter = mActions.expectGeckoEvent("Reader:Added");
        mSolo.clickLongOnView(readerIcon);
        eventData = contentReaderAddedExpecter.blockForEventData();
        isAdded(eventData);
        contentReaderAddedExpecter.unregisterListener();

        // Waiting for the favicon since is the last element loaded usually
        faviconExpecter = mActions.expectGeckoEvent("Reader:FaviconRequest");
        contentPageShowExpecter = mActions.expectGeckoEvent("Content:PageShow");
        readerIcon = getReaderIcon();
        paintExpecter = mActions.expectPaint();
        mSolo.clickOnView(readerIcon);

        // Changing devices orientation to be sure that all devices are in portrait when will access the reader toolbar
        mSolo.setActivityOrientation(Solo.PORTRAIT);
        faviconExpecter.blockForEvent();
        faviconExpecter.unregisterListener();
        contentPageShowExpecter.blockForEvent();
        contentPageShowExpecter.unregisterListener();
        paintExpecter.blockUntilClear(EVENT_CLEAR_DELAY_MS);
        paintExpecter.unregisterListener();
        verifyPageTitle("Robocop Text Page");

        // Open the share menu for the reader toolbar
        height = mDriver.getGeckoTop() + mDriver.getGeckoHeight() - 10;
        width = mDriver.getGeckoLeft() + mDriver.getGeckoWidth() - 10;
        mAsserter.dumpLog("Long Clicking at width = " + String.valueOf(width) + " and height = " + String.valueOf(height));
        mSolo.clickOnScreen(width,height);
        mAsserter.ok(mSolo.waitForText("Share via"), "Waiting for the share menu", "The share menu is present");
        mActions.sendSpecialKey(Actions.SpecialKey.BACK); // Close the share menu

        // Remove page from the Reading List using reader toolbar
        height = mDriver.getGeckoTop() + mDriver.getGeckoHeight() - 10;
        width = mDriver.getGeckoLeft() + 50;
        mAsserter.dumpLog("Long Clicking at width = " + String.valueOf(width) + " and height = " + String.valueOf(height));
        mSolo.clickOnScreen(width,height);
        mAsserter.ok(mSolo.waitForText("Page removed from your Reading List"), "Waiting for the page to removed from your Reading List", "The page is removed from your Reading List");

        //Add page to the Reading List using reader toolbar
        mSolo.clickOnScreen(width,height);
        mAsserter.ok(mSolo.waitForText("Page added to your Reading List"), "Waiting for the page to be added to your Reading List", "The page was added to your Reading List");

        // Open the Reading List menu for the toolbar
        height = mDriver.getGeckoTop() + mDriver.getGeckoHeight() - 10;
        width = mDriver.getGeckoLeft() + mDriver.getGeckoWidth()/2 - 10;
        mAsserter.dumpLog("Long Clicking at width = " + String.valueOf(width) + " and height = " + String.valueOf(height));
        contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");
        mSolo.clickOnScreen(width,height);
        contentEventExpecter.blockForEvent();
        contentEventExpecter.unregisterListener();

        // Check if the page is present in the Reading List
        mAsserter.ok(mSolo.waitForText("Robocop Text Page"), "Verify if the page is added to your Reading List", "The page is present in your Reading List");

        // Check if the page is added in History tab like a Reading List item
        openAboutHomeTab(AboutHomeTabs.MOST_RECENT);
        list = findListViewWithTag("most_recent");
        child = list.getChildAt(1);
        mAsserter.ok(child != null, "item can be retrieved", child != null ? child.toString() : "null!");
        mSolo.clickLongOnView(child);
        mAsserter.ok(mSolo.waitForText("Open in Reader"), "Verify if the page is present in history as a Reading List item", "The page is present in history as a Reading List item");
        mActions.sendSpecialKey(Actions.SpecialKey.BACK); // Dismiss the context menu
        mSolo.waitForText("Robocop Text Page");

        // Verify separately the Reading List entries for tablets and phone because for tablets there is an extra child in UI design
        if (devType.equals("phone")) {
            childNo = 1;
        }
        else {
            childNo = 2;
        }
        // Verify if the page is present to your Reading List
        openAboutHomeTab(AboutHomeTabs.READING_LIST);
        list = findListViewWithTag("reading_list");
        child = list.getChildAt(childNo-1);
        mAsserter.ok(child != null, "Verify if the page is present to your Reading List", "The page is present in your Reading List");
        contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");
        mSolo.clickOnView(child);
        contentEventExpecter.blockForEvent();
        contentEventExpecter.unregisterListener();
        verifyPageTitle("Robocop Text Page");

        // Verify that we are in reader mode and remove the page from Reading List
        height = mDriver.getGeckoTop() + mDriver.getGeckoHeight() - 10;
        width = mDriver.getGeckoLeft() + 50;
        mAsserter.dumpLog("Long Clicking at width = " + String.valueOf(width) + " and height = " + String.valueOf(height));
        mSolo.clickOnScreen(width,height);
        mAsserter.ok(mSolo.waitForText("Page removed from your Reading List"), "Waiting for the page to removed from your Reading List", "The page is removed from your Reading List");
        verifyPageTitle("Robocop Text Page");

        //Check if the Reading List is empty
        openAboutHomeTab(AboutHomeTabs.READING_LIST);
        list = findListViewWithTag("reading_list");
        child = list.getChildAt(childNo-1);
        mAsserter.ok(child == null, "Verify if the Reading List is empty", "The Reading List is empty");
    }

    // Get the reader icon method
    protected View getReaderIcon() {
        View pageActionLayout = mSolo.getView(0x7f070025);
        final ViewGroup actionLayoutEntry = (ViewGroup)pageActionLayout;
        View icon = actionLayoutEntry.getChildAt(1);
        if (icon == null || icon.getVisibility() != View.VISIBLE) {
            // wait for the view to be visible, otherwise it may not respond
            // to clicks -- see bug 927578
            mAsserter.dumpLog("reader icon not visible -- waiting for visibility");
            Condition visibilityCondition = new Condition() {
                @Override
                public boolean isSatisfied() {
                    View conditionIcon = actionLayoutEntry.getChildAt(1);
                    if (conditionIcon == null || 
                        conditionIcon.getVisibility() != View.VISIBLE)
                        return false;
                    return true;
                }
            };
            waitForCondition(visibilityCondition, READER_ICON_MAX_WAIT_MS);
            icon = actionLayoutEntry.getChildAt(1);
            mAsserter.ok(icon != null, "checking reader icon view", "reader icon view not null");
            mAsserter.ok(icon.getVisibility() == View.VISIBLE, "checking reader icon visible", "reader icon visible");
        }
        return icon;
    }

    // This method check to see if a reader item is added to the reader list
    private boolean isAdded(String eventData) {
        try {
            JSONObject data = new JSONObject(eventData);
                if (data.getInt("result") == 0) {
                    mAsserter.ok(true, "Waiting for the page to be added to your Reading List", "The page was added to your Reading List");
                }
                else {
                    if (data.getInt("result") == 2) {
                        mAsserter.ok(true, "Trying to add a second time the page in your Reading List", "The page is already in your Reading List");
                    }
                }
        } catch (JSONException e) {
            mAsserter.ok(false, "Error parsing the event data", e.toString());
            return false;
        }
        return true;
    }
}
