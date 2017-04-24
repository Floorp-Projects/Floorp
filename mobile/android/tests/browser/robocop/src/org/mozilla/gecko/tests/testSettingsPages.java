/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

/* Tests the working of Settings menu:
 *  Traverses through every setting item and tests it.
 */

public class testSettingsPages extends BaseTest {

    public void testSettingsPages() {
        blockForGeckoReady();

        // Open Settings Menu
        selectMenuItem(mStringHelper.SETTINGS_LABEL);
        // Assert that we are inside the Settings screen by checking if "General" section label is present and enabled
        String itemName = "^" + mStringHelper.GENERAL_SECTION_LABEL + "$";
        mAsserter.ok(waitForEnabledText(itemName), "Waiting for menu item " + itemName, itemName + " is present and enabled");

        // Now traverse the Settings menu items one by one.
        testGeneralSection();
        testSearchSection();
        testPrivacySection();
        testAccessibilitySection();
        testNotificationsSection();
        testAdvancedSection();
        testClearPrivateDataSection();
        testMozillaSection();
    }

    private void testGeneralSection() {
        selectSettingsSection(mStringHelper.GENERAL_SECTION_LABEL);
        /* Assert that screen was successfully opened by searching any one label.
         * For example, check "Language" label for "General" section */
        String itemName = "^" + mStringHelper.LANGUAGE_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    private void testSearchSection() {
        selectSettingsSection(mStringHelper.SEARCH_SECTION_LABEL);
        String itemName = "^" + mStringHelper.SHOW_SEARCH_HISTORY_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    private void testPrivacySection() {
        selectSettingsSection(mStringHelper.PRIVACY_SECTION_LABEL);
        String itemName = "^" + mStringHelper.DO_NOT_TRACK_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    private void testAccessibilitySection() {
        selectSettingsSection(mStringHelper.ACCESSIBILITY_SECTION_LABEL);
        String itemName = "^" + mStringHelper.ALWAYS_ZOOM_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    private void testNotificationsSection() {
        selectSettingsSection(mStringHelper.NOTIFICATIONS_SECTION_LABEL);
        String itemName = "^" + mStringHelper.NEW_IN_FIREFOX_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    private void testAdvancedSection() {
        selectSettingsSection(mStringHelper.ADVANCED_SECTION_LABEL);
        String itemName = "^" + mStringHelper.RESTORE_TABS_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    private void testClearPrivateDataSection() {
        selectSettingsSection(mStringHelper.CLEAR_PRIVATE_DATA_SECTION_LABEL);
        String itemName = "^" + mStringHelper.SITE_SETTINGS_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    private void testMozillaSection() {
        selectSettingsSection(mStringHelper.MOZILLA_SECTION_LABEL);
        String itemName = "^" + mStringHelper.FAQS_LABEL + "$";
        mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find label " + itemName, itemName + " found");
        mSolo.goBack();
    }

    /**
     * Select <sectionName> from Settings.
     */
    private void selectSettingsSection(String sectionName) {
        if (!sectionName.isEmpty()) {
            String itemName = "^" + sectionName + "$";
            mAsserter.ok(waitForPreferencesText(itemName), "Waiting for and scrolling once to find section " + itemName, itemName + " found");
            mAsserter.ok(waitForEnabledText(itemName), "Waiting for enabled text " + itemName, itemName + " option is present and enabled");
            mSolo.clickOnText(itemName);
        }
    }

}
