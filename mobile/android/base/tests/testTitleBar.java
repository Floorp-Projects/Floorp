package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.util.HardwareUtils;

/**
 * This patch tests the option that shows the full URL and title in the URL Bar
 */

public class testTitleBar extends PixelTest {
    public void testTitleBar() {
        // Because there is no title bar option on new tablet, we don't need to run this test.
        if (HardwareUtils.isTablet()) {
            return;
        }

        blockForGeckoReady();
        checkOption();
    }

    public void checkOption() {

        String blank1 = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String title = mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE;

        // Loading a page
        loadUrlAndWait(blank1);
        verifyUrlBarTitle(blank1);

        // Ensure the title is displayed in the URL Bar
        selectOption(mStringHelper.SHOW_PAGE_TITLE_LABEL);
        loadUrlAndWait(blank1);
        verifyUrlBarTitle(title);

        // Ensure the full URL is displayed in the URL Bar
        selectOption(mStringHelper.SHOW_PAGE_ADDRESS_LABEL);
        loadUrlAndWait(blank1);
        verifyUrlBarTitle(blank1);
    }

    // Entering settings, changing the options: show title/page address option and verifing the device type because for phone there is an extra back action to exit the settings menu
    public void selectOption(String option) {
        selectSettingsItem(mStringHelper.DISPLAY_SECTION_LABEL, mStringHelper.TITLE_BAR_LABEL);
        mAsserter.ok(waitForText(mStringHelper.SHOW_PAGE_TITLE_LABEL), "Waiting for the pop-up to open", "Pop up with the options was openend");
        mSolo.clickOnText(option);
        mAsserter.ok(waitForText(mStringHelper.CHARACTER_ENCODING_LABEL), "Waiting to press the option", "The pop-up is dismissed once clicked");
        if (mDevice.type.equals("phone")) {
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            mAsserter.ok(waitForText(mStringHelper.CUSTOMIZE_SECTION_LABEL), "Waiting to perform one back", "One back performed");
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            mAsserter.ok(waitForText(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL), "Waiting to exit settings", "Exit settings done");
        }
        else {
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            mAsserter.ok(waitForText(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL), "Waiting to exit settings", "Exit settings done");
        }
    }
}
