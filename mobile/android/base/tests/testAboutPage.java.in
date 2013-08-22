#filter substitution
package @ANDROID_PACKAGE_NAME@.tests;

import @ANDROID_PACKAGE_NAME@.*;

/* Tests related to the about: page:
 *  - check that about: loads from the URL bar
 *  - check that about: loads from Settings/About...
 */
public class testAboutPage extends PixelTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testAboutPage() {
        blockForGeckoReady();

        // Load the about: page and verify its title
        String url = "about:";
        loadAndPaint(url);

        Element urlBarTitle = mDriver.findElement(getActivity(), URL_BAR_TITLE_ID);
        mAsserter.isnot(urlBarTitle, null, "Got the URL bar title");
        assertMatches(urlBarTitle.getText(), "About (Fennec|Nightly|Aurora|Firefox|Firefox Beta)", "page title match");

        // Open a new page to remove the about: page from the current tab
        url = getAbsoluteUrl("/robocop/robocop_blank_01.html");
        inputAndLoadUrl(url);

        // Set up listeners to catch the page load we're about to do
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        Actions.EventExpecter contentEventExpecter = mActions.expectGeckoEvent("DOMContentLoaded");

        selectSettingsItem("Mozilla", "About (Fennec|Nightly|Aurora|Firefox|Firefox Beta)");

        // Wait for the new tab and page to load
        tabEventExpecter.blockForEvent();
        contentEventExpecter.blockForEvent();

        tabEventExpecter.unregisterListener();
        contentEventExpecter.unregisterListener();

        // Grab the title to make sure the about: page was loaded
        urlBarTitle = mDriver.findElement(getActivity(), URL_BAR_TITLE_ID);
        mAsserter.isnot(urlBarTitle, null, "Got the URL bar title");
        assertMatches(urlBarTitle.getText(), "About (Fennec|Nightly|Aurora|Firefox|Firefox Beta)", "page title match");
    }
}
