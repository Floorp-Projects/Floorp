package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

/** This patch tests the System Pages first by loading system pages from
 *  the awesome bar and then from Firefox menu 
 */
public class testSystemPages extends PixelTest {
    final int mExpectedTabCount = 1;
    private static final int AFTER_BACK_SLEEP_MS = 500;

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testSystemPages() {
        blockForGeckoReady();

        String urls [] = { "about:firefox", "about:rights", "about:addons", "about:downloads", "about:buildconfig", "about:feedback", "about:healthreport", "about:" };
        // Pages to be tested from the menu and their expected urls. This if of the form { {{ <path to item> }, { <expected url> }}* }
        String menuItems [][][] = {{{ "Apps" }, { "about:apps" }},
                                  {{ "Downloads" }, { "about:downloads" }},
                                  {{ "Add-ons" }, { "about:addons" }},
                                  {{ "Settings", "Mozilla", "About (Fennec|Nightly|Aurora|Firefox|Firefox Beta)" }, { "about:" }},
                                  {{ "Settings", "Mozilla", "Give feedback" }, { "about:feedback" }},
                                  {{ "Settings", "Mozilla", "View my Health Report" }, { "about:healthreport" }}};

        /* Load system pages from url and check that the pages are loaded in the same tab */
        checkUrl(urls);

        /* Verify that the search field is not in the focus by pressing back. That will load the previous
           about: page if there is no the keyboard to dismiss, meaning that the search field was not in focus */
        loadAndPaint("about:about");

        // Press back to verify if the keyboard is dismissed or the previous about: page loads
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
        // may not get a paint on Back...pause briefly to make sure it completes
        mSolo.sleep(AFTER_BACK_SLEEP_MS);

        // We will use the "about:" page as our reference page.
        loadAndPaint("about:");
        verifyUrl("about:"); // Verify that the previous about: page is loaded, meaning no keyboard was present

        // Load system pages by navigating through the UI.
        loadItemsByLevel(menuItems);
    }

    // Load from Url the about: pages,verify the Url and the tabs number
    public void checkUrl(String urls []) {
        for (String url:urls) {
            loadAndPaint(url);
            verifyTabCount(mExpectedTabCount);
            verifyUrl(url);
        }
    }

    public void loadItemsByLevel(String[][][] menuItems) {
        Actions.EventExpecter tabEventExpecter;
        Actions.EventExpecter contentEventExpecter;
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        int expectedTabCount = mExpectedTabCount;
        // There's some special casing for about: because it's our starting page.
        for (String[][] item : menuItems) {
            String [] pathToItem = item[0];
            String expectedUrl = item[1][0];

            expectedTabCount++;

            // Set up listeners to catch the page load we're about to do
            tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
            contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");
            selectMenuItemByPath(pathToItem);

            // Wait for the new tab and page to load
            if ("about:".equals(expectedUrl)) {
                waitForPaint(paintExpecter); // Waiting for the page to load
                paintExpecter.unregisterListener();
            } else {
                tabEventExpecter.blockForEvent();
                contentEventExpecter.blockForEvent();
            }
            tabEventExpecter.unregisterListener();
            contentEventExpecter.unregisterListener();

            verifyUrl(expectedUrl);
            if ("about:".equals(expectedUrl)) {
                // Decreasing because we do not expect this to be in a different tab.
                expectedTabCount--;
            }
            verifyTabCount(expectedTabCount);
        }
    }
}
