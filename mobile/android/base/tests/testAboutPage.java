package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

/* Tests related to the about: page:
 *  - check that about: loads from the URL bar
 *  - check that about: loads from Settings/About...
 */
public class testAboutPage extends PixelTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    private void ensureTitleMatches(final String regex) {
        Element urlBarTitle = mDriver.findElement(getActivity(), URL_BAR_TITLE_ID);
        mAsserter.isnot(urlBarTitle, null, "Got the URL bar title");
        assertMatches(urlBarTitle.getText(), regex, "page title match");
    }

    public void testAboutPage() {
        blockForGeckoReady();

        // Load the about: page and verify its title.
        String url = "about:";
        loadAndPaint(url);

        ensureTitleMatches("About (Fennec|Nightly|Aurora|Firefox|Firefox Beta)");

        // Open a new page to remove the about: page from the current tab.
        url = getAbsoluteUrl("/robocop/robocop_blank_01.html");
        inputAndLoadUrl(url);

        // At this point the page title should have been set.
        ensureTitleMatches("Browser Blank Page 01");

        // Set up listeners to catch the page load we're about to do.
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        Actions.EventExpecter contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");

        selectSettingsItem("Mozilla", "About (Fennec|Nightly|Aurora|Firefox|Firefox Beta)");

        // Wait for the new tab and page to load
        tabEventExpecter.blockForEvent();
        contentEventExpecter.blockForEvent();

        tabEventExpecter.unregisterListener();
        contentEventExpecter.unregisterListener();

        // Grab the title to make sure the about: page was loaded.
        ensureTitleMatches("About (Fennec|Nightly|Aurora|Firefox|Firefox Beta)");
    }
}
