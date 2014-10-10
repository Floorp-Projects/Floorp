package org.mozilla.gecko.tests;
import org.mozilla.gecko.Actions;

/**
 * This patch tests the option that shows the full URL and title in the URL Bar
 */

public class testTitleBar extends PixelTest {
    public void testTitleBar() {
        blockForGeckoReady();
        checkOption();
    }

    public void checkOption() {

        String blank1 = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String title = StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE;

        // Loading a page
        inputAndLoadUrl(blank1);
        verifyPageTitle(title);

        // Verifing the full URL is displayed in the URL Bar
        selectOption(StringHelper.SHOW_PAGE_ADDRESS_LABEL);
        inputAndLoadUrl(blank1);
        verifyUrl(blank1);

        // Verifing the title is displayed in the URL Bar
        selectOption(StringHelper.SHOW_PAGE_TITLE_LABEL);
        inputAndLoadUrl(blank1);
        verifyPageTitle(title);
    }

    // Entering settings, changing the options: show title/page address option and verifing the device type because for phone there is an extra back action to exit the settings menu
    public void selectOption(String option) {
        selectSettingsItem(StringHelper.DISPLAY_SECTION_LABEL, StringHelper.TITLE_BAR_LABEL);
        mAsserter.ok(waitForText(StringHelper.SHOW_PAGE_TITLE_LABEL), "Waiting for the pop-up to open", "Pop up with the options was openend");
        mSolo.clickOnText(option);
        mAsserter.ok(waitForText(StringHelper.CHARACTER_ENCODING_LABEL), "Waiting to press the option", "The pop-up is dismissed once clicked");
        if (mDevice.type.equals("phone")) {
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            mAsserter.ok(waitForText(StringHelper.CUSTOMIZE_SECTION_LABEL), "Waiting to perform one back", "One back performed");
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            mAsserter.ok(waitForText(StringHelper.ROBOCOP_BLANK_PAGE_01_URL), "Waiting to exit settings", "Exit settings done");
        }
        else {
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            mAsserter.ok(waitForText(StringHelper.ROBOCOP_BLANK_PAGE_01_URL), "Waiting to exit settings", "Exit settings done");
        }
    }
}
