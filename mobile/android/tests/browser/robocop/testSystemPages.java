package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.AppConstants;

/** This patch tests the System Pages first by loading system pages from
 *  the awesome bar and then from Firefox menu
 */
public class testSystemPages extends PixelTest {
    final int mExpectedTabCount = 1;
    private static final int AFTER_BACK_SLEEP_MS = 500;

    public void testSystemPages() {
        blockForGeckoReady();

        final String urls [] = { mStringHelper.ABOUT_FIREFOX_URL, mStringHelper.ABOUT_RIGHTS_URL,
                mStringHelper.ABOUT_ADDONS_URL, mStringHelper.ABOUT_DOWNLOADS_URL, StringHelper.ABOUT_LOGINS_URL,
                mStringHelper.ABOUT_BUILDCONFIG_URL, mStringHelper.ABOUT_FEEDBACK_URL,
                mStringHelper.ABOUT_HEALTHREPORT_URL, mStringHelper.ABOUT_SCHEME
        };
        // Pages to be tested from the menu and their expected urls. This if of the form { {{ <path to item> }, { <expected url> }}* }
        String menuItems [][][] = {{{ mStringHelper.DOWNLOADS_LABEL }, { mStringHelper.ABOUT_DOWNLOADS_URL}},
                                  {{ mStringHelper.LOGINS_LABEL}, { StringHelper.ABOUT_LOGINS_URL }},
                                  {{ mStringHelper.ADDONS_LABEL }, { mStringHelper.ABOUT_ADDONS_URL }},
                                  {{ mStringHelper.SETTINGS_LABEL, mStringHelper.MOZILLA_SECTION_LABEL, mStringHelper.ABOUT_LABEL }, { mStringHelper.ABOUT_SCHEME }},
                                  {{ mStringHelper.SETTINGS_LABEL, mStringHelper.MOZILLA_SECTION_LABEL, mStringHelper.FEEDBACK_LABEL }, { mStringHelper.ABOUT_FEEDBACK_URL }},
                                  {{ mStringHelper.SETTINGS_LABEL, mStringHelper.MOZILLA_SECTION_LABEL, mStringHelper.MY_HEALTH_REPORT_LABEL }, { mStringHelper.ABOUT_HEALTHREPORT_URL }}};

        /* Load system pages from url and check that the pages are loaded in the same tab */
        checkUrl(urls);

        /* Verify that the search field is not in the focus by pressing back. That will load the previous
           about: page if there is no the keyboard to dismiss, meaning that the search field was not in focus */
        loadAndPaint(mStringHelper.ABOUT_ABOUT_URL);

        // Press back to verify if the keyboard is dismissed or the previous about: page loads
        mSolo.goBack();
        // may not get a paint on Back...pause briefly to make sure it completes
        mSolo.sleep(AFTER_BACK_SLEEP_MS);

        // We will use the "about:" page as our reference page.
        loadAndPaint(mStringHelper.ABOUT_SCHEME);
        verifyUrl(mStringHelper.ABOUT_SCHEME); // Verify that the previous about: page is loaded, meaning no keyboard was present

        // Load system pages by navigating through the UI.
        loadItemsByLevel(menuItems);
    }

    // Load from Url the about: pages,verify the Url and the tabs number
    public void checkUrl(String urls []) {
        for (String url:urls) {
            if (skipItemURL(url)) {
                continue;
            }
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

            if (skipItemURL(expectedUrl)) {
                continue;
            }

            expectedTabCount++;

            // Set up listeners to catch the page load we're about to do
            tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
            contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");
            selectMenuItemByPath(pathToItem);

            // Wait for the new tab and page to load
            if (mStringHelper.ABOUT_SCHEME.equals(expectedUrl)) {
                waitForPaint(paintExpecter); // Waiting for the page to load
                paintExpecter.unregisterListener();
            } else {
                tabEventExpecter.blockForEvent();
                contentEventExpecter.blockForEvent();
            }
            tabEventExpecter.unregisterListener();
            contentEventExpecter.unregisterListener();

            verifyUrl(expectedUrl);
            if (mStringHelper.ABOUT_SCHEME.equals(expectedUrl)) {
                // Decreasing because we do not expect this to be in a different tab.
                expectedTabCount--;
            }
            verifyTabCount(expectedTabCount);
        }
    }

    private boolean skipItemURL(String item) {
        if (StringHelper.ABOUT_LOGINS_URL.equals(item) && !AppConstants.NIGHTLY_BUILD) {
            return true;
        }
        return false;
    }
}
