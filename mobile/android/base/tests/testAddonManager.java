package org.mozilla.gecko.tests;

import org.json.JSONObject;
import org.mozilla.gecko.Actions;

import android.util.DisplayMetrics;

/**
 * This test performs the following steps to check the behavior of the Add-on Manager:
 *
 * 1) Open the Add-on Manager from the Add-ons menu item, and then close it.
 * 2) Open the Add-on Manager by visiting about:addons in the URL bar.
 * 3) Open a new tab, select the Add-ons menu item, then verify that the existing
 *    Add-on Manager tab was selected, instead of opening a new tab.
 */
public class testAddonManager extends PixelTest  {
    public void testAddonManager() {
        Actions.EventExpecter tabEventExpecter;
        Actions.EventExpecter contentEventExpecter;
        final String aboutAddonsURL = StringHelper.ABOUT_ADDONS_URL;

        blockForGeckoReady();

        // Use the menu to open the Addon Manger
        selectMenuItem(StringHelper.ADDONS_LABEL);

        // Set up listeners to catch the page load we're about to do
        tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");

        // Wait for the new tab and page to load
        tabEventExpecter.blockForEvent();
        contentEventExpecter.blockForEvent();

        tabEventExpecter.unregisterListener();
        contentEventExpecter.unregisterListener();

        // Verify the url
        verifyPageTitle(StringHelper.ADDONS_LABEL, aboutAddonsURL);

        // Close the Add-on Manager
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);

        // Load the about:addons page and verify it was loaded
        loadAndPaint(aboutAddonsURL);
        verifyPageTitle(StringHelper.ADDONS_LABEL, aboutAddonsURL);

        // Setup wait for tab to spawn and load
        tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");

        // Open a new tab
        final String blankURL = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        addTab(blankURL);

        // Wait for the new tab and page to load
        tabEventExpecter.blockForEvent();
        contentEventExpecter.blockForEvent();

        tabEventExpecter.unregisterListener();
        contentEventExpecter.unregisterListener();

        // Verify tab count has increased
        verifyTabCount(2);

        // Verify the page was opened
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE, blankURL);

        // Addons Manager is not opened 2 separate times when opened from the menu
        selectMenuItem(StringHelper.ADDONS_LABEL);

        // Verify tab count not increased
        verifyTabCount(2);
    }
}
