package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;

/* Tests related to the about: page:
 *  - check that about: loads from the URL bar
 *  - check that about: loads from Settings/About...
 */
public class testAboutPage extends PixelTest {
    private void ensureTitleMatches(final String regex) {
        Element urlBarTitle = mDriver.findElement(getActivity(), R.id.url_bar_title);
        mAsserter.isnot(urlBarTitle, null, "Got the URL bar title");
        assertMatches(urlBarTitle.getText(), regex, "page title match");
    }

    public void testAboutPage() {
        blockForGeckoReady();

        // Load the about: page and verify its title.
        String url = "about:";
        loadAndPaint(url);

        ensureTitleMatches(StringHelper.ABOUT_LABEL);

        // Open a new page to remove the about: page from the current tab.
        url = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        inputAndLoadUrl(url);

        // At this point the page title should have been set.
        ensureTitleMatches(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);

        // Set up listeners to catch the page load we're about to do.
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        Actions.EventExpecter contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");

        selectSettingsItem(StringHelper.MOZILLA_SECTION_LABEL, StringHelper.ABOUT_LABEL);

        // Wait for the new tab and page to load
        tabEventExpecter.blockForEvent();
        contentEventExpecter.blockForEvent();

        tabEventExpecter.unregisterListener();
        contentEventExpecter.unregisterListener();

        // Grab the title to make sure the about: page was loaded.
        ensureTitleMatches(StringHelper.ABOUT_LABEL);
    }
}
