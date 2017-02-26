package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.GeckoClickHelper;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;

public class testTabStrip extends UITest {
    public void testTabStrip() {
        if (!DeviceHelper.isTablet()) {
            return;
        }

        GeckoHelper.blockForReady();

        testOpenPrivateTabInNormalMode();
        // This test depends on ROBOCOP_BIG_LINK_URL being loaded in tab 0 from the first test.
        testNewNormalTabScroll();
    }

    /**
     * Make sure that a private tab opened while the tab strip is in normal mode does not get added
     * to the tab strip (bug 1339066).
    */
    private void testOpenPrivateTabInNormalMode() {
        final String normalModeUrl = mStringHelper.ROBOCOP_BIG_LINK_URL;
        NavigationHelper.enterAndLoadUrl(normalModeUrl);

        final Actions.EventExpecter titleExpecter = mActions.expectGlobalEvent(Actions.EventType.UI, "Content:DOMTitleChanged");
        GeckoClickHelper.openCentralizedLinkInNewPrivateTab();

        titleExpecter.blockForEvent();
        titleExpecter.unregisterListener();
        // In the passing version of this test the UI shouldn't change at all in response to the
        // new private tab, but to prevent a false positive when the private tab does get added to
        // the tab strip in error, sleep here to make sure the UI has time to update before we
        // check it.
        mSolo.sleep(250);

        // Now make sure there's still only one tab in the tab strip, and that it's still the normal
        // mode tab.

        mTabStrip.assertTabCountIs(1);
        mToolbar.assertTitle(normalModeUrl);
    }

    /**
     * Test that we do *not* scroll to a new normal tab opened from a link in a page (bug 1340929).
     * Assumes ROBOCOP_BIG_LINK_URL is loaded in tab 0.
     */
    private void testNewNormalTabScroll() {
        mTabStrip.fillStripWithTabs();
        mTabStrip.switchToTab(0);

        final int tabZeroId = mTabStrip.getTabViewAtVisualIndex(0).getTabId();

        final int tabCountBeforeNewTab = mTabStrip.getTabCount();
        GeckoClickHelper.openCentralizedLinkInNewTab();
        mTabStrip.waitForNewTab(tabCountBeforeNewTab);

        // If we scrolled to the new tab then the first tab visible on screen will no longer be the
        // tabs list tab 0.
        fAssertEquals("Current first tab is tabs list first tab", tabZeroId, mTabStrip.getTabViewAtVisualIndex(0).getTabId());
    }
 }
