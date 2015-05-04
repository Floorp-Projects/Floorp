/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

public class testHomeBanner extends UITest {

    private static final String TEST_URL = "chrome://roboextender/content/robocop_home_banner.html";
    private static final String TEXT = "The quick brown fox jumps over the lazy dog.";

    public void testHomeBanner() {
        GeckoHelper.blockForReady();

        // Make sure the banner is not visible to start.
        mAboutHome.assertVisible()
                  .assertBannerNotVisible();

        // These test methods depend on being run in this order.
        addBannerTest();

        // Make sure the banner hides when the user starts interacting with the url bar.
        hideOnToolbarFocusTest();

        // Make sure to test dismissing the banner after everything else, since dismissing
        // the banner will prevent it from showing up again.
        dismissBannerTest();
    }

    /**
     * Adds a banner message, verifies that it appears when it should, and verifies that
     * onshown/onclick handlers are called in JS.
     *
     * Note: This test does not remove the message after it is done.
     */
    private void addBannerTest() {
        // Load about:home and make sure the onshown handler is called.
        Actions.EventExpecter eventExpecter = getActions().expectGeckoEvent("TestHomeBanner:MessageShown");
        addBannerMessage();
        NavigationHelper.enterAndLoadUrl(mStringHelper.ABOUT_HOME_URL);
        eventExpecter.blockForEvent();

        // Verify that the banner is visible with the correct text.
        mAboutHome.assertBannerText(TEXT);

        // Verify that the banner isn't visible after navigating away from about:home.
        NavigationHelper.enterAndLoadUrl(mStringHelper.ABOUT_FIREFOX_URL);
        mAboutHome.assertBannerNotVisible();
    }


    private void hideOnToolbarFocusTest() {
        NavigationHelper.enterAndLoadUrl(mStringHelper.ABOUT_HOME_URL);
        mAboutHome.assertVisible()
                  .assertBannerVisible();

        mToolbar.enterEditingMode();
        mAboutHome.assertBannerNotVisible();

        mToolbar.dismissEditingMode();
        mAboutHome.assertBannerVisible();
    }

    /**
     * Adds a banner message, verifies that its ondismiss handler is called in JS,
     * and verifies that the banner is no longer shown after it is dismissed.
     *
     * Note: This test does not remove the message after it is done.
     */
    private void dismissBannerTest() {
        NavigationHelper.enterAndLoadUrl(mStringHelper.ABOUT_HOME_URL);
        mAboutHome.assertVisible();

        // Test to make sure the ondismiss handler is called when the close button is clicked.
        final Actions.EventExpecter eventExpecter = getActions().expectGeckoEvent("TestHomeBanner:MessageDismissed");
        mAboutHome.dismissBanner();
        eventExpecter.blockForEvent();

        mAboutHome.assertBannerNotVisible();
    }

    /**
     * Loads the roboextender page to add a message to the banner.
     */
    private void addBannerMessage() {
        final Actions.EventExpecter eventExpecter = getActions().expectGeckoEvent("TestHomeBanner:MessageAdded");
        NavigationHelper.enterAndLoadUrl(TEST_URL + "#addMessage");
        eventExpecter.blockForEvent();
    }
}
