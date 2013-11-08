package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

public class testHomeBanner extends BaseTest {

    private static final String TEST_URL = "chrome://roboextender/content/robocop_home_banner.html";
    private static final String TEXT = "The quick brown fox jumps over the lazy dog.";

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testHomeBanner() {
        blockForGeckoReady();

        Actions.EventExpecter eventExpecter = mActions.expectGeckoEvent("TestHomeBanner:MessageAdded");

        // Add a message to the home banner
        inputAndLoadUrl(TEST_URL + "#addMessage");
        eventExpecter.blockForEvent();

        // Load about:home, and test to make sure the onshown handler is called
        eventExpecter = mActions.expectGeckoEvent("TestHomeBanner:MessageShown");
        inputAndLoadUrl("about:home");
        eventExpecter.blockForEvent();

        // Verify that the correct message text showed up in the banner
        mAsserter.ok(waitForText(TEXT), "banner text", "correct text appeared in the home banner");

        // Test to make sure the onclick handler is called
        eventExpecter = mActions.expectGeckoEvent("TestHomeBanner:MessageClicked");
        mSolo.clickOnText(TEXT);
        eventExpecter.blockForEvent();
    }
}
