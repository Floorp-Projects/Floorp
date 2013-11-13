package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

/* This patch tests the Master Password feature first by enabling the password,
then testing it on a login page and finally disabling the password */
public class testMasterPassword extends PixelTest {
    Device dev;

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testMasterPassword() {
        blockForGeckoReady();

        dev = new Device();
        String password = ("Good");
        String badPassword = ("Bad");

        enableMasterPassword(password, badPassword);
        verifyLoginPage(password, badPassword);
        disableMasterPassword(password, badPassword);
    }

    public void enableMasterPassword(String password, String badPassword) {

        // Look for the 'Settings' menu if this device/OS uses it
        selectSettingsItem("Privacy", "Use master password");
        waitForText("^Create Master Password$");

        // Verify that the OK button is not activated until both fields are filled
        closeTabletKeyboard();
        mAsserter.ok(!mSolo.getButton("OK").isEnabled(), "Verify if the OK button is inactive", "The OK button is inactive until both fields are filled");

        // Verify that the OK button is not activated until the Confirm password field is filled
        editPasswordField(0, password);
        mAsserter.ok(!mSolo.getButton("OK").isEnabled(), "Verify if the OK button is inactive", "The OK button is inactive until the Confirm password field is filled");

        // Verify that the OK button is not activated until both fields contain the same password
        editPasswordField(1, badPassword);
        mAsserter.ok(!mSolo.getButton("OK").isEnabled(), "Verify if the OK button is inactive", "The OK button is inactive until both fields contain the same password");

        // Verify that the OK button is not activated until the Password field is filled
        mSolo.clearEditText(0);
        mAsserter.ok(!mSolo.getButton("OK").isEnabled(), "Verify if the OK button is inactive", "The OK button is inactive until the Password field is filled");

        // Check that the Master Password is not set when canceling the action
        mSolo.clickOnEditText(0);
        mActions.sendKeys(password);
        mSolo.clearEditText(1);
        mSolo.clickOnEditText(1);
        mActions.sendKeys(password);
        waitForText("^Cancel$");
        mSolo.clickOnText("^Cancel$");
        waitForText("^Use master password$");
        mSolo.clickOnText("^Use master password$");
        mAsserter.ok(mSolo.waitForText("^Create Master Password$"), "Checking if no password was set if the action was canceled", "No password was set");

        // Enable Master Password
        mSolo.clickOnEditText(0);
        mActions.sendKeys(password);
        mSolo.clickOnEditText(1);
        mActions.sendKeys(password);

        // Verify that the input characters are converted to dots automatically
        mAsserter.ok(waitForText("."), "waiting to convert the letters in dots", "The letters are converted in dots");
        mSolo.clickOnButton("OK");

        // Verify that the Master Password was set
        mSolo.searchText("Privacy");
        mAsserter.ok(mSolo.waitForText("^Use master password$"), "Checking if Use master password is present", "Use master password is present");
        mSolo.clickOnText("^Use master password$");
        mAsserter.ok(mSolo.waitForText("Remove Master Password"), "Checking if the password is enabled", "The password is enabled");
        clickOnButton("Cancel"); // Go back to settings menu

        if ("phone".equals(mDevice.type)) {
            // Phones don't have headers like tablets, so we need to pop up one more level.
            waitForText("Use master password");
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
        }
        waitForText("Settings");
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);// Close the Settings Menu
    }

    public void disableMasterPassword(String password, String badPassword) {

        // Look for the 'Settings' menu if this device/OS uses it
        selectSettingsItem("Privacy", "Use master password");
        waitForText("^Remove Master Password$");

        // Verify that the OK button is not activated if the password field is empty
        closeTabletKeyboard();
        mAsserter.ok(!mSolo.getButton("OK").isEnabled(), "Verify if the OK button is inactive", "The OK button is inactive if the password field is empty");

        // Verify that the OK button is activated if the password field contains characters
        editPasswordField(0, badPassword);
        mAsserter.ok(mSolo.getButton("OK").isEnabled(), "Verify if the OK button is activated", "The OK button is activated even if the wrong password is filled");
        mSolo.clickOnButton("OK");
        mAsserter.ok(mSolo.waitForText("^Incorrect password$"), "Waiting for Incorrect password notification", "The Incorrect password notification appears");

        // Disable Master Password
        mSolo.clickOnText("^Use master password$");
        waitForText("^Remove Master Password$");
        closeTabletKeyboard();
        editPasswordField(0, password);
        mSolo.clickOnButton("OK");

        // Verify that the Master Password was disabled
        mSolo.searchText("Privacy");
        mAsserter.ok(mSolo.waitForText("^Use master password$"), "Checking if Use master password is present", "Use master password is present");
        mSolo.clickOnText("^Use master password$");
        mAsserter.ok(waitForText("^Create Master Password$"), "Checking if the password is disabled", "The password is disabled");
        clickOnButton("Cancel"); // Go back to settings menu
    }

    public void editPasswordField(int i, String password) {
        mSolo.clickOnEditText(i);
        mActions.sendKeys(password);
        toggleVKB(); // Don't use BACK; this will close the password dialog on devices with hardware keyboard.
    }

    public void noDoorhangerDisplayed(String LOGIN_URL) {
        waitForText("Browser Blank Page 01|Enter Search or Address");
        inputAndLoadUrl(LOGIN_URL);
        mAsserter.is(waitForText("Save password for"), false, "Doorhanger notification is hidden");
    }

    public void doorhangerDisplayed(String LOGIN_URL) {
        waitForText("Browser Blank Page 01|Enter Search or Address");
        inputAndLoadUrl(LOGIN_URL);
        mAsserter.is(mSolo.waitForText("Save password for"), true, "Doorhanger notification is displayed");
    }

    // Checks to see if the device is a Tablet, because for those devices we need an extra back action to close the keyboard
    public void closeTabletKeyboard() {
        if (dev.type.equals("tablet")) {
            mSolo.sleep(1500);
            toggleVKB();// Close the keyboard for tablets
        }
    }

    public void clearPrivateData() {

        // Look for the 'Settings' menu if this device/OS uses it
        selectSettingsItem("Privacy", "Clear private data");

        waitForText("Browsing & download history"); // Make sure the Clear private data pop-up is displayed
        Actions.EventExpecter clearPrivateDataEventExpecter = mActions.expectGeckoEvent("Sanitize:Finished");
        if (mSolo.searchText("Clear data") && !mSolo.searchText("Cookies")) {
            mSolo.clickOnText("^Clear data$");
            clearPrivateDataEventExpecter.blockForEvent();
        } else { // For some reason the pop-up was not opened
            if (mSolo.searchText("Cookies")) {
                mSolo.clickOnText("^Clear private data$");
                waitForText("Browsing & download history"); // Make sure the Clear private data pop-up is displayed
                mSolo.clickOnText("^Clear data$");
                clearPrivateDataEventExpecter.blockForEvent();
            } else {
                mAsserter.ok(false, "Something happened and the clear data dialog could not be opened", "Failed to clear data");
            }
        }

        // Check that the Master Password isn't disabled by clearing private data
        waitForText("^Use master password$");
        mSolo.clickOnText("^Use master password$");
        mAsserter.ok(mSolo.searchText("^Remove Master Password$"), "Checking if the master password was disabled by clearing private data", "The master password is not disabled by clearing private data");
        clickOnButton("Cancel"); // Close the Master Password menu

        if ("phone".equals(mDevice.type)) {
            // Phones don't have headers like tablets, so we need to pop up one more level.
            waitForText("Use master password");
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
        }
        waitForText("Settings");
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);// Close the Settings Menu
        // Make sure the settings menu has been closed.
        mAsserter.ok(mSolo.waitForText("Browser Blank Page 01"), "Waiting for blank browser page after exiting settings", "Blank browser page present");
    }

    public void verifyLoginPage(String password, String badPassword) {
        String LOGIN_URL = getAbsoluteUrl("/robocop/robocop_login.html");
        String option [] = {"Save", "Don't save"};

        doorhangerDisplayed(LOGIN_URL);// Check that the doorhanger is displayed
        for (String item:option) {
            if (item.equals("Save")) {
                mAsserter.ok(mSolo.waitForText("Save"), "Checking if Save option is present", "Save option is present");
                mSolo.clickOnButton(item);

                // Verify that the Master Password isn't deactivated when the password field is empty
                closeTabletKeyboard();
                mSolo.clickOnButton("OK");

                // Verify that the Master Password isn't deactivated when using the wrong password
                closeTabletKeyboard();
                editPasswordField(0, badPassword);
                mSolo.clickOnButton("OK");

                // Verify that the Master Password is deactivated when using the right password
                closeTabletKeyboard();
                editPasswordField(0, password);
                mSolo.clickOnButton("OK");

                // Verify that the Master Password is triggered once per session
                noDoorhangerDisplayed(LOGIN_URL);// Check that the doorhanger isn't displayed
            } else {
                clearPrivateData();
                doorhangerDisplayed(LOGIN_URL);// Check that the doorhanger is displayed
                mAsserter.ok(mSolo.waitForText("Don't save"), "Checking if Don't save option is present again", "Don't save option is present again");
                mSolo.clickOnText("Don't save");
                doorhangerDisplayed(LOGIN_URL);// Check that the doorhanger is displayed again
                mAsserter.ok(mSolo.waitForText("Don't save"), "Checking if Don't save option is present again", "Don't save option is present again");
                mSolo.clickOnText("Don't save");
                // Make sure the settings menu has been closed.
                mAsserter.ok(mSolo.waitForText("Browser Blank Page 01"), "Waiting for blank browser page after exiting settings", "Blank browser page present");
            }
        }
    }
}
