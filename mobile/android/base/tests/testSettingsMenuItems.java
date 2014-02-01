package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.HashMap;

/** This patch tests the Sections present in the Settings Menu and the
 *  default values for them
 */
public class testSettingsMenuItems extends PixelTest {
    int mMidWidth;
    int mMidHeight;
    String BRAND_NAME = "(Fennec|Nightly|Aurora|Firefox|Firefox Beta)";

    /**
     * The following String[][] (arrays) match the menu hierarchy for each section.
     * Each String[] (array) represents the menu items/choices in the following order:
     *
     * itemTitle { defaultValue [options] }
     *
     * where defaultValue is optional, and there can be multiple options.
     *
     * These menu items are the ones that are always present - to test menu items that differ
     * based on build (e.g., release vs. nightly), add the items in <code>addConditionalSettings</code>. 
     */

    // Customize menu items.
    String[][] OPTIONS_CUSTOMIZE = {
        { "Home", "", "Panels" },
        { "Search", "", "Show search suggestions", "Installed search engines"},
        { "Tabs", "Don't restore after quitting " + BRAND_NAME, "Always restore", "Don't restore after quitting " + BRAND_NAME },
        { "Import from Android", "", "Bookmarks", "History", "Import" },
    };

    // Display menu items.
    String[][] OPTIONS_DISPLAY = {
        { "Text size" },
        { "Title bar", "Show page title", "Show page title", "Show page address" },
        { "Advanced" },
        { "Character encoding", "Don't show menu", "Show menu", "Don't show menu" },
        { "Plugins", "Tap to play", "Enabled", "Tap to play", "Disabled" },
    };

    // Privacy menu items.
    String[][] OPTIONS_PRIVACY = {
        { "Tracking", "Do not tell sites anything about my tracking preferences", "Tell sites that I do not want to be tracked", "Tell sites that I want to be tracked", "Do not tell sites anything about my tracking preferences" },
        { "Cookies", "Enabled", "Enabled, excluding 3rd party", "Disabled" },
        { "Remember passwords" },
        { "Use master password" },
        { "Clear private data", "", "Browsing & download history", "Downloaded files", "Form & search history", "Cookies & active logins", "Saved passwords", "Cache", "Offline website data", "Site settings", "Clear data" },
    };

    String[][] OPTIONS_MOZILLA = {
        { "About " + BRAND_NAME },
        { "FAQs" },
        { "Give feedback" },
        { "Show product announcements" },
        { "Data choices" },
        { BRAND_NAME + " Health Report", "Shares data with Mozilla about your browser health and helps you understand your browser performance" },
        { "View my Health Report" },
    };

    /*
     * This sets up a hierarchy of settings to test.
     *
     * The keys are the top-level settings categories, and each value is a
     * List of menu items contained within each category.
     *
     * Each menu item is itself an array as follows:
     *  - item title
     *  - default string value of item (optional)
     *  - string values of options that are displayed once clicked (optional).
     */
    public void setupSettingsMap(Map<String, List<String[]>> settingsMap) {
        settingsMap.put("Customize", new ArrayList<String[]>(Arrays.asList(OPTIONS_CUSTOMIZE)));
        settingsMap.put("Display", new ArrayList<String[]>(Arrays.asList(OPTIONS_DISPLAY)));
        settingsMap.put("Privacy", new ArrayList<String[]>(Arrays.asList(OPTIONS_PRIVACY)));
        settingsMap.put("Mozilla", new ArrayList<String[]>(Arrays.asList(OPTIONS_MOZILLA)));
    }

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testSettingsMenuItems() {
        blockForGeckoReady();
        mMidWidth = mDriver.getGeckoWidth()/2;
        mMidHeight = mDriver.getGeckoHeight()/2;

        Map<String, List<String[]>> settingsMenuItems = new HashMap<String, List<String[]>>();
        setupSettingsMap(settingsMenuItems);

        // Set special handling for Settings items that are conditionally built.
        addConditionalSettings(settingsMenuItems);

        selectMenuItem("Settings");
        waitForText("Settings");

        // Dismiss the Settings screen and verify that the view is returned to about:home page
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);

        // Waiting for page title to appear to be sure that is fully loaded before opening the menu
        waitForText("Enter Search");
        verifyUrl("about:home");

        selectMenuItem("Settings");
        waitForText("Settings");

        checkForSync(mDevice);

        checkMenuHierarchy(settingsMenuItems);
    }

    /**
     * Check for Sync in settings.
     *
     * Sync location is a top level menu item on phones, but is under "Customize" on tablets.
     *
     */
    public void checkForSync(Device device) {
        if (device.type.equals("tablet")) {
            // Select "Customize" from settings.
            String customizeString = "^Customize$";
            waitForEnabledText(customizeString);
            mSolo.clickOnText(customizeString);
        }
        mAsserter.ok(mSolo.waitForText("Sync"), "Waiting for Sync option", "The Sync option is present");
    }

    /**
     * Check for conditions for building certain settings, and add them to be tested
     * if they are present.
     */
    public void addConditionalSettings(Map<String, List<String[]>> settingsMap) {
        // Preferences dependent on RELEASE_BUILD
        if (!AppConstants.RELEASE_BUILD) {
            // Text reflow - only built if *not* release build
            String[] textReflowUi = { "Text reflow" };
            settingsMap.get("Display").add(textReflowUi);

            // Anonymous cell tower/wifi collection - only built if *not* release build
            String[] networkReportingUi = { "Mozilla location services", "Help improve geolocation services for the Open Web by letting " + BRAND_NAME + " collect and send anonymous cellular tower data" };
            settingsMap.get("Mozilla").add(networkReportingUi);

        }

        // Automatic updates
        if (AppConstants.MOZ_UPDATER) {
            String[] autoUpdateUi = { "Download updates automatically", "Only over Wi-Fi", "Always", "Only over Wi-Fi", "Never" };
            settingsMap.get("Customize").add(autoUpdateUi);
        }

        // Crash reporter
        if (AppConstants.MOZ_CRASHREPORTER) {
            String[] crashReporterUi = { "Crash Reporter", BRAND_NAME + " submits crash reports to help Mozilla make your browser more stable and secure" };
            settingsMap.get("Mozilla").add(crashReporterUi);
        }

        // Telemetry
        if (AppConstants.MOZ_TELEMETRY_REPORTING) {
            String[] telemetryUi = { "Telemetry", "Shares performance, usage, hardware and customization data about your browser with Mozilla to help us make " + BRAND_NAME + " better" };
            settingsMap.get("Mozilla").add(telemetryUi);
        }
    }

    public void checkMenuHierarchy(Map<String, List<String[]>> settingsMap) {
        // Check the items within each category.
        for (Entry<String, List<String[]>> e : settingsMap.entrySet()) {
            String section = "^" + e.getKey() + "$";
            List<String[]> sectionItems = e.getValue();

            waitForEnabledText(section);
            mSolo.clickOnText(section);

            // Check each item of the section.
            for (String[] item : sectionItems) {
                int itemLen = item.length;

                // Each item must at least have a title.
                mAsserter.ok(item.length > 0, "Section-item", "Each item must at least have a title");

                // Check item title.
                String itemTitle = "^" + item[0] + "$";
                boolean foundText = waitExtraForText(itemTitle);
                mAsserter.ok(foundText, "Waiting for settings item " + itemTitle + " in section " + section,
                             "The " + itemTitle + " option is present in section " + section);
                // Check item default, if it exists.
                if (itemLen > 1) {
                    String itemDefault = "^" + item[1] + "$";
                    foundText = waitExtraForText(itemDefault);
                    mAsserter.ok(foundText, "Waiting for settings item default " + itemDefault
                                 + " in section " + section,
                                 "The " + itemDefault + " default is present in section " + section);
                }
                // Check item choices, if they exist.
                if (itemLen > 2) {
                    waitForEnabledText(itemTitle);
                    mSolo.clickOnText(itemTitle);
                    for (int i = 2; i < itemLen; i++) {
                        String itemChoice = "^" + item[i] + "$";
                        foundText = waitExtraForText(itemChoice);
                        mAsserter.ok(foundText, "Waiting for settings item choice " + itemChoice
                                     + " in section " + section,
                                     "The " + itemChoice + " choice is present in section " + section);
                    }
                    // Leave submenu after checking.
                    if (waitForText("^Cancel$")) {
                        mSolo.clickOnText("^Cancel$");
                    } else {
                        // Some submenus aren't dialogs, but are nested screens; exit using "back".
                        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
                    }
                }
            }
            // Navigate back a screen if on a phone.
            if (mDevice.type.equals("phone")) {
                // Click back to return to previous menu. Tablets shouldn't do this because they use headers and fragments.
                mActions.sendSpecialKey(Actions.SpecialKey.BACK);
            }
        }
    }

    // Solo.waitForText usually scrolls down in a view when text is not visible.
    // In this test, Solo.waitForText scrolling does not work, so we use this
    // hack to do the same thing.
    private boolean waitExtraForText(String txt) {
        boolean foundText = waitForText(txt);
        if (!foundText) {
            // If we don't see the item, scroll down once in case it's off-screen.
            // Hacky way to scroll down.  solo.scroll* does not work in dialogs.
            MotionEventHelper meh = new MotionEventHelper(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop());
            meh.dragSync(mMidWidth, mMidHeight+100, mMidWidth, mMidHeight-100);

            foundText = mSolo.waitForText(txt);
        }
        return foundText;
    }
}
