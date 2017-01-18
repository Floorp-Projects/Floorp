/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.components.AppMenuComponent;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;
import org.mozilla.gecko.util.GeckoBundle;

import com.robotium.solo.Solo;

/**
 * Set of tests to test UI App menu and submenus the user interact with.
 */
public class testAppMenuPathways extends UITest {

    /**
     * Robocop supports only a single test function per test class. Therefore, we
     * have a single top-level test function that dispatches to sub-tests.
     */
    public void testAppMenuPathways() {
        GeckoHelper.blockForReady();

        _testHardwareMenuKeyOpenClose();
        _testSaveAsPDFPathway();
    }

    public void _testHardwareMenuKeyOpenClose() {
        mAppMenu.assertMenuIsNotOpen();

        mSolo.sendKey(Solo.MENU);
        mAppMenu.waitForMenuOpen();
        mAppMenu.assertMenuIsOpen();

        mSolo.sendKey(Solo.MENU);
        mAppMenu.waitForMenuClose();
        mAppMenu.assertMenuIsNotOpen();
    }

    public void _testSaveAsPDFPathway() {
        // Page menu should be disabled in about:home.
        mAppMenu.assertMenuItemIsDisabledAndVisible(AppMenuComponent.PageMenuItem.SAVE_AS_PDF);

        // Generate a mock Content:LocationChange message with video mime-type for the current tab (tabId = 0).
        final GeckoBundle message = new GeckoBundle();
        message.putString("contentType", "video/webm");
        message.putString("baseDomain", "webmfiles.org");
        message.putBoolean("sameDocument", false);
        message.putString("userRequested", "");
        message.putString("uri", getAbsoluteIpUrl("/big-buck-bunny_trailer.webm"));
        message.putInt("tabID", 0);

        // Mock video playback with the generated message and Content:LocationChange event.
        getActions().sendGlobalEvent("Content:LocationChange", message);

        // Save as pdf menu is disabled while playing video.
        mAppMenu.assertMenuItemIsDisabledAndVisible(AppMenuComponent.PageMenuItem.SAVE_AS_PDF);

        // The above mock video playback test changes Java state, but not the associated JS state.
        // Navigate to a new page so that the Java state is cleared.
        NavigationHelper.enterAndLoadUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        mToolbar.assertTitle(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);

        // Test save as pdf functionality.
        // The following call doesn't wait for the resulting pdf but checks that no exception are thrown.
        // NOTE: save as pdf functionality must be done at the end as it is slow and cause other test operations to fail.
        mAppMenu.pressMenuItem(AppMenuComponent.PageMenuItem.SAVE_AS_PDF);
    }
}
