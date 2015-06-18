/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.R;

import com.jayway.android.robotium.solo.Condition;
import android.view.View;

/**
 * This patch tests the clear private data options:
 * - clear history option by: adding and checking that clear private
 * data option removes the history items but not the users bookmarks
 * - clear site settings and clear saved password by: checking
 * each option present in the doorhanger and clearing the settings from
 * the URL bar context menu and settings menu
 */

public class testClearPrivateData extends PixelTest {
    private final int TEST_WAIT_MS = 10000;

    public void testClearPrivateData() {
        blockForGeckoReady();
        clearHistory();
        clearSiteSettings();
        clearPassword();
    }

    private void clearHistory() {

        // Loading a page and adding a second one as bookmark to have user made bookmarks and history
        String blank1 = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String blank2 = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        String title = mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE;

        loadUrlAndWait(blank1);
        verifyUrlBarTitle(blank1);
        mDatabaseHelper.addMobileBookmark(mStringHelper.ROBOCOP_BLANK_PAGE_02_TITLE, blank2);

        // Checking that the history list is not empty
        verifyHistoryCount(1);

        //clear and check for device
        checkDevice(title, blank1);

        // Checking that history list is empty
        verifyHistoryCount(0);

        // Checking that the user made bookmark is not removed
        mAsserter.ok(mDatabaseHelper.isBookmark(blank2), "Checking that bookmarks have not been removed", "User made bookmarks were not removed with private data");
    }

    private void verifyHistoryCount(final int expectedCount) {
        boolean match = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.HISTORY).size() == expectedCount;
            }
        }, TEST_WAIT_MS);
        mAsserter.ok(match, "Checking that the number of history items is correct", String.valueOf(expectedCount) + " history items present in the database");
    }

    public void clearSiteSettings() {
        String shareStrings[] = {"Share your location with", "Share", "Don't share", "There are no settings to clear"};
        String titleGeolocation = mStringHelper.ROBOCOP_GEOLOCATION_TITLE;
        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_GEOLOCATION_URL);
        loadCheckDismiss(shareStrings[1], url, shareStrings[0]);
        checkOption(shareStrings[1], "Clear");
        checkOption(shareStrings[3], "Cancel");
        loadCheckDismiss(shareStrings[2], url, shareStrings[0]);
        checkOption(shareStrings[2], "Cancel");
        checkDevice(titleGeolocation, url);
    }

    public void clearPassword(){
        String passwordStrings[] = { mStringHelper.LOGIN_MESSAGE, mStringHelper.LOGIN_ALLOW, mStringHelper.LOGIN_DENY };
        String title = mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE;
        String loginUrl = getAbsoluteUrl(mStringHelper.ROBOCOP_LOGIN_01_URL);

        loadCheckDismiss(passwordStrings[1], loginUrl, passwordStrings[0]);
        checkOption(mStringHelper.CONTEXT_MENU_SITE_SETTINGS_SAVE_PASSWORD, "Clear");
        loadCheckDismiss(passwordStrings[2], loginUrl, passwordStrings[0]);
        checkDevice(title, getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL));
    }

    // clear private data and verify the device type because for phone there is an extra back action to exit the settings menu
    public void checkDevice(final String title, final String url) {
        clearPrivateData();
        if (mDevice.type.equals("phone")) {
            mSolo.goBack();
            mAsserter.ok(waitForText(mStringHelper.PRIVACY_SECTION_LABEL), "waiting to perform one back", "one back");
        }
        mSolo.goBack();
        verifyUrlBarTitle(url);
    }

    // Load a URL, verify that the doorhanger appears and dismiss it
    public void loadCheckDismiss(String option, String url, String message) {
        loadUrlAndWait(url);
        waitForText(message);
        mAsserter.is(mSolo.searchText(message), true, "Doorhanger:" + message + " has been displayed");
        mSolo.clickOnButton(option);
        mAsserter.is(mSolo.searchText(message), false, "Doorhanger:" + message + " has been hidden");
    }

    //Verify if there are settings to be clear if so clear them from the URL bar context menu
    public void checkOption(String option, String button) {
        if (mDevice.version.equals("2.x")) {
            // Use the context menu in pre-11
            final View toolbarView = mSolo.getView(R.id.browser_toolbar);
            mSolo.clickLongOnView(toolbarView);
            mAsserter.ok(waitForText(mStringHelper.CONTEXT_MENU_ITEMS_IN_URL_BAR[2]), "Waiting for the pop-up to open", "Pop up was opened");
        } else {
            // Use the Page menu in 11+
            selectMenuItem(mStringHelper.PAGE_LABEL);
            mAsserter.ok(waitForText(mStringHelper.CONTEXT_MENU_ITEMS_IN_URL_BAR[2]), "Waiting for the submenu to open", "Submenu was opened");
        }

	mSolo.clickOnText(mStringHelper.CONTEXT_MENU_ITEMS_IN_URL_BAR[2]);
        mAsserter.ok(waitForText(option), "Verify that the option: " + option + " is in the list", "The option is in the list. There are settings to clear");
        mSolo.clickOnButton(button);
    }
}
