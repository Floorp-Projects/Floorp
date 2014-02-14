package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.tests.helpers.*;

import android.view.View;

public class testHomeBanner extends UITest {

    private static final String TEST_URL = "chrome://roboextender/content/robocop_home_banner.html";
    private static final String TEXT = "The quick brown fox jumps over the lazy dog.";

    public void testHomeBanner() {
        GeckoHelper.blockForReady();

        // Make sure the banner is not visible.
        mAboutHome.assertVisible()
                  .assertBannerNotVisible();

        addBannerMessage();

        // Load about:home again, and make sure the onshown handler is called.
        Actions.EventExpecter eventExpecter = getActions().expectGeckoEvent("TestHomeBanner:MessageShown");
        NavigationHelper.enterAndLoadUrl("about:home");
        eventExpecter.blockForEvent();

        // Verify that the banner is visible with the correct text.
        mAboutHome.assertBannerText(TEXT);

        // Test to make sure the onclick handler is called.
        eventExpecter = getActions().expectGeckoEvent("TestHomeBanner:MessageClicked");
        mAboutHome.clickOnBanner();
        eventExpecter.blockForEvent();

        // Verify that the banner isn't visible after navigating away from about:home.
        NavigationHelper.enterAndLoadUrl("about:firefox");

        // AboutHomeComponent calls mSolo.getView, which will fail an assertion if the
        // view is not present, so we need to use findViewById in this case.
        final View banner = getActivity().findViewById(R.id.home_banner);
        assertTrue("The HomeBanner is not visible", banner == null || banner.getVisibility() != View.VISIBLE);

        removeBannerMessage();

        // Verify that the banner no longer appears.
        NavigationHelper.enterAndLoadUrl("about:home");
        mAboutHome.assertVisible()
                  .assertBannerNotVisible();
    }

    /**
     * Loads the roboextender page to add a message to the banner.
     */
    private void addBannerMessage() {
        final Actions.EventExpecter eventExpecter = getActions().expectGeckoEvent("TestHomeBanner:MessageAdded");
        NavigationHelper.enterAndLoadUrl(TEST_URL + "#addMessage");
        eventExpecter.blockForEvent();
    }

    /**
     * Loads the roboextender page to remove the message from the banner.
     */
    private void removeBannerMessage() {
        final Actions.EventExpecter eventExpecter = getActions().expectGeckoEvent("TestHomeBanner:MessageRemoved");
        NavigationHelper.enterAndLoadUrl(TEST_URL + "#removeMessage");
        eventExpecter.blockForEvent();
    }
}
