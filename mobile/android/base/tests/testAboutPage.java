package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.NewTabletUI;
import org.mozilla.gecko.R;

import android.app.Activity;

/* Tests related to the about: page:
 *  - check that about: loads from the URL bar
 *  - check that about: loads from Settings/About...
 */
public class testAboutPage extends PixelTest {
    /**
     * Ensures the page title matches the given regex (as opposed to String equality).
     */
    private void ensureTitleMatches(final String titleRegex, final String urlRegex) {
        final Activity activity = getActivity();
        final Element urlBarTitle = mDriver.findElement(activity, R.id.url_bar_title);

        // TODO: We should also be testing what the page title preference value is.
        final String expectedTitle = NewTabletUI.isEnabled(activity) ? urlRegex : titleRegex;
        mAsserter.isnot(urlBarTitle, null, "Got the URL bar title");
        assertMatches(urlBarTitle.getText(), expectedTitle, "page title match");
    }

    public void testAboutPage() {
        blockForGeckoReady();

        // Load the about: page and verify its title.
        String url = StringHelper.ABOUT_SCHEME;
        loadAndPaint(url);

        ensureTitleMatches(StringHelper.ABOUT_LABEL, url);

        // Open a new page to remove the about: page from the current tab.
        url = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        inputAndLoadUrl(url);

        // At this point the page title should have been set.
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE, url);

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
        ensureTitleMatches(StringHelper.ABOUT_LABEL, StringHelper.ABOUT_SCHEME);
    }
}
