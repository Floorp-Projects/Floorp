package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.GeckoClickHelper;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;

/**
 * Make sure that a private tab opened while the tab strip is in normal mode does not get added to
 * the tab strip (bug 1339066).
 */
public class testTabStripPrivacyMode extends UITest {
    public void testTabStripPrivacyMode() {
        if (!DeviceHelper.isTablet()) {
            return;
        }

        GeckoHelper.blockForReady();

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

        mTabStrip.assertTabCount(1);
        mToolbar.assertTitle(normalModeUrl);
    }
 }
