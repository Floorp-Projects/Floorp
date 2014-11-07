/* This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.tests;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.mozstumbler.service.AppGlobals;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import com.jayway.android.robotium.solo.Condition;

/*
 * This test enables (checkbox checked) the Fennec setting to contribute to MLS, then waits for
 * a response Intent from the stumbler service to confirm it has started. Then, it disables the
 * service in the setting, and waits for confirmation that the servie has stopped.
 */
public class testStumblerSetting extends BaseTest {
    boolean mIsEnabled;

    public void testStumblerSetting() {
        if (!AppConstants.MOZ_STUMBLER_BUILD_TIME_ENABLED) {
            mAsserter.info("Checking stumbler build config.", "Skipping test as Stumbler is not enabled in this build.");
            return;
        }

        blockForGeckoReady();

        selectMenuItem(StringHelper.SETTINGS_LABEL);
        mAsserter.ok(mSolo.waitForText(StringHelper.SETTINGS_LABEL),
                "The Settings menu did not load", StringHelper.SETTINGS_LABEL);

        String section = "^" + StringHelper.MOZILLA_SECTION_LABEL + "$";
        waitForEnabledText(section);
        mSolo.clickOnText(section);

        String itemTitle = "^" + StringHelper.LOCATION_SERVICES_LABEL + "$";
        boolean foundText = waitForPreferencesText(itemTitle);
        mAsserter.ok(foundText, "Waiting for settings item " + itemTitle + " in section " + section,
                "The " + itemTitle + " option is present in section " + section);

        BroadcastReceiver enabledDisabledReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent.getAction().equals(AppGlobals.ACTION_TEST_SETTING_ENABLED)) {
                    mIsEnabled = true;
                } else {
                    mIsEnabled = false;
                }
            }
        };

        Context context = getInstrumentation().getTargetContext();
        IntentFilter intentFilter = new IntentFilter(AppGlobals.ACTION_TEST_SETTING_ENABLED);
        intentFilter.addAction(AppGlobals.ACTION_TEST_SETTING_DISABLED);
        context.registerReceiver(enabledDisabledReceiver, intentFilter);

        boolean checked = mSolo.isCheckBoxChecked(itemTitle);
        try {
            mAsserter.ok(!checked, "Checking stumbler setting is unchecked.", "Unchecked as expected.");

            waitForEnabledText(itemTitle);
            mSolo.clickOnText(itemTitle);

            mSolo.waitForCondition(new Condition() {
                @Override
                public boolean isSatisfied() {
                    return mIsEnabled;
                }
            }, 15000);

            mAsserter.ok(mIsEnabled, "Checking if stumbler became enabled.", "Stumbler is enabled.");
            mSolo.clickOnText(itemTitle);

            mSolo.waitForCondition(new Condition() {
                @Override
                public boolean isSatisfied() {
                    return !mIsEnabled;
                }
            }, 15000);

            mAsserter.ok(!mIsEnabled, "Checking if stumbler became disabled.", "Stumbler is disabled.");
        } finally {
            context.unregisterReceiver(enabledDisabledReceiver);
        }
    }
}
