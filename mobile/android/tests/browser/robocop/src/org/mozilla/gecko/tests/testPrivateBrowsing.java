/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.ArrayList;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Tabs;

/**
 * The test loads a new private tab and loads a page with a big link on it
 * Opens the link in a new private tab and checks that it is private
 * Adds a new normal tab and loads a 3rd URL
 * Checks that the bigLinkUrl loaded in the normal tab is present in the browsing history but the 2 urls opened in private tabs are not
 */
public class testPrivateBrowsing extends ContentContextMenuTest {

    public void testPrivateBrowsing() {
        String bigLinkUrl = getAbsoluteUrl(mStringHelper.ROBOCOP_BIG_LINK_URL);
        String blank1Url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String blank2Url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        Tabs tabs = Tabs.getInstance();

        blockForGeckoReady();

        Actions.EventExpecter tabExpecter = mActions.expectGeckoEvent("Tab:Added");
        Actions.EventExpecter contentExpecter = mActions.expectGeckoEvent("Content:PageShow");
        tabs.loadUrl(bigLinkUrl, Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_PRIVATE);
        tabExpecter.blockForEvent();
        tabExpecter.unregisterListener();
        contentExpecter.blockForEvent();
        contentExpecter.unregisterListener();
        verifyTabCount(1);

        // May intermittently get context menu for normal tab without additional wait
        mSolo.sleep(5000);

        // Open the link context menu and verify the options
        verifyContextMenuItems(mStringHelper.CONTEXT_MENU_ITEMS_IN_PRIVATE_TAB);

        // Check that "Open Link in New Tab" is not in the menu
        mAsserter.ok(!mSolo.searchText(mStringHelper.CONTEXT_MENU_ITEMS_IN_NORMAL_TAB[0]), "Checking that 'Open Link in New Tab' is not displayed in the context menu", "'Open Link in New Tab' is not displayed in the context menu");

        // Open the link in a new private tab and check that it is private
        tabExpecter = mActions.expectGeckoEvent("Tab:Added");
        contentExpecter = mActions.expectGeckoEvent("Content:PageShow");
        mSolo.clickOnText(mStringHelper.CONTEXT_MENU_ITEMS_IN_PRIVATE_TAB[0]);
        String eventData = tabExpecter.blockForEventData();
        tabExpecter.unregisterListener();
        contentExpecter.blockForEvent();
        contentExpecter.unregisterListener();
        mAsserter.ok(isTabPrivate(eventData), "Checking if the new tab opened from the context menu was a private tab", "The tab was a private tab");
        verifyTabCount(2);

        // Open a normal tab to check later that it was registered in the Firefox Browser History
        tabExpecter = mActions.expectGeckoEvent("Tab:Added");
        contentExpecter = mActions.expectGeckoEvent("Content:PageShow");
        tabs.loadUrl(blank2Url, Tabs.LOADURL_NEW_TAB);
        tabExpecter.blockForEvent();
        tabExpecter.unregisterListener();
        contentExpecter.blockForEvent();
        contentExpecter.unregisterListener();
        verifyTabCount(2);

        // wait for history updates to complete
        mSolo.sleep(3000);

        // Get the history list and check that the links open in private browsing are not saved
        final ArrayList<String> firefoxHistory = mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.HISTORY);

        mAsserter.ok(!firefoxHistory.contains(bigLinkUrl), "Check that the link opened in the first private tab was not saved", bigLinkUrl + " was not added to history");
        mAsserter.ok(!firefoxHistory.contains(blank1Url), "Check that the link opened in the private tab from the context menu was not saved", blank1Url + " was not added to history");
        mAsserter.ok(firefoxHistory.contains(blank2Url), "Check that the link opened in the normal tab was saved", blank2Url + " was added to history");
    }

    private boolean isTabPrivate(String eventData) {
        try {
            JSONObject data = new JSONObject(eventData);
            return data.getBoolean("isPrivate");
        } catch (JSONException e) {
            mAsserter.ok(false, "Error parsing the event data", e.toString());
            return false;
        }
    }
}
