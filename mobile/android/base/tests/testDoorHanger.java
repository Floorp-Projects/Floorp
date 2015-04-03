/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.widget.CheckBox;
import android.view.View;
import com.jayway.android.robotium.solo.Condition;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Actions;

/* This test will test if doorhangers are displayed and dismissed
   The test will test:
   * geolocation doorhangers - sharing and not sharing the location dismisses the doorhanger
   * opening a new tab hides the doorhanger
   * offline storage permission doorhangers - allowing and not allowing offline storage dismisses the doorhanger
   * Password Manager doorhangers - Remember and Not Now options dismiss the doorhanger
*/
public class testDoorHanger extends BaseTest {
    public void testDoorHanger() {
        String GEO_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_GEOLOCATION_URL);
        String BLANK_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String OFFLINE_STORAGE_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_OFFLINE_STORAGE_URL);

        blockForGeckoReady();

        // Test geolocation notification
        loadUrlAndWait(GEO_URL);
        waitForText(mStringHelper.GEO_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.GEO_MESSAGE), true, "Geolocation doorhanger has been displayed");

        // Test "Share" button hides the notification
        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(mStringHelper.GEO_ALLOW);
        waitForTextDismissed(mStringHelper.GEO_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.GEO_MESSAGE), false, "Geolocation doorhanger has been hidden when allowing share");

        // Re-trigger geolocation notification
        loadUrlAndWait(GEO_URL);
        waitForText(mStringHelper.GEO_MESSAGE);

        // Test "Don't share" button hides the notification
        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(mStringHelper.GEO_DENY);
        waitForTextDismissed(mStringHelper.GEO_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.GEO_MESSAGE), false, "Geolocation doorhanger has been hidden when denying share");

        /* FIXME: disabled on fig - bug 880060 (for some reason this fails because of some raciness)
        // Re-trigger geolocation notification
        loadUrlAndWait(GEO_URL);
        waitForText(GEO_MESSAGE);

        // Add a new tab
        addTab(BLANK_URL);

        // Make sure doorhanger is hidden
        mAsserter.is(mSolo.searchText(GEO_MESSAGE), false, "Geolocation doorhanger notification is hidden when opening a new tab");
        */

        boolean offlineAllowedByDefault = true;
        // Save offline-allow-by-default preferences first
        final String[] prefNames = { "offline-apps.allow_by_default" };
        final int ourRequestId = 0x7357;
        final Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
        mActions.sendPreferencesGetEvent(ourRequestId, prefNames);
        try {
            JSONObject data = null;
            int requestId = -1;

            // Wait until we get the correct "Preferences:Data" event
            while (requestId != ourRequestId) {
                data = new JSONObject(eventExpecter.blockForEventData());
                requestId = data.getInt("requestId");
            }
            eventExpecter.unregisterListener();

            JSONArray preferences = data.getJSONArray("preferences");
            if (preferences.length() > 0) {
                JSONObject pref = (JSONObject) preferences.get(0);
                offlineAllowedByDefault = pref.getBoolean("value");
            }

            // Turn off offline-allow-by-default
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", "offline-apps.allow_by_default");
            jsonPref.put("type", "bool");
            jsonPref.put("value", false);
            setPreferenceAndWaitForChange(jsonPref);
        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting preference", e.toString());
        }

        // Load offline storage page
        loadUrlAndWait(OFFLINE_STORAGE_URL);
        waitForText(mStringHelper.OFFLINE_MESSAGE);

        // Test doorhanger dismissed when tapping "Don't share"
        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(mStringHelper.OFFLINE_DENY);
        waitForTextDismissed(mStringHelper.OFFLINE_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.OFFLINE_MESSAGE), false, "Offline storage doorhanger notification is hidden when denying storage");

        // Load offline storage page
        loadUrlAndWait(OFFLINE_STORAGE_URL);
        waitForText(mStringHelper.OFFLINE_MESSAGE);

        // Test doorhanger dismissed when tapping "Allow" and is not displayed again
        mSolo.clickOnButton(mStringHelper.OFFLINE_ALLOW);
        waitForTextDismissed(mStringHelper.OFFLINE_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.OFFLINE_MESSAGE), false, "Offline storage doorhanger notification is hidden when allowing storage");
        loadUrlAndWait(OFFLINE_STORAGE_URL);
        mAsserter.is(mSolo.searchText(mStringHelper.OFFLINE_MESSAGE), false, "Offline storage doorhanger is no longer triggered");

        try {
            // Revert offline setting
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", "offline-apps.allow_by_default");
            jsonPref.put("type", "bool");
            jsonPref.put("value", offlineAllowedByDefault);
            setPreferenceAndWaitForChange(jsonPref);
        } catch (JSONException e) {
            mAsserter.ok(false, "exception setting preference", e.toString());
        }

        // Load new login page
        loadUrlAndWait(getAbsoluteUrl(mStringHelper.ROBOCOP_LOGIN_01_URL));
        waitForText(mStringHelper.LOGIN_MESSAGE);

        // Test doorhanger is dismissed when tapping "Remember".
        mSolo.clickOnButton(mStringHelper.LOGIN_ALLOW);
        waitForTextDismissed(mStringHelper.LOGIN_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.LOGIN_MESSAGE), false, "Login doorhanger notification is hidden when allowing saving password");

        // Load login page
        loadUrlAndWait(getAbsoluteUrl(mStringHelper.ROBOCOP_LOGIN_02_URL));
        waitForText(mStringHelper.LOGIN_MESSAGE);

        // Test doorhanger is dismissed when tapping "Never".
        mSolo.clickOnButton(mStringHelper.LOGIN_DENY);
        waitForTextDismissed(mStringHelper.LOGIN_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.LOGIN_MESSAGE), false, "Login doorhanger notification is hidden when denying saving password");

        testPopupBlocking();
    }

    private void testPopupBlocking() {
        String POPUP_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_POPUP_URL);

        try {
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", "dom.disable_open_during_load");
            jsonPref.put("type", "bool");
            jsonPref.put("value", true);
            setPreferenceAndWaitForChange(jsonPref);
        } catch (JSONException e) {
            mAsserter.ok(false, "exception setting preference", e.toString());
        }

        // Load page with popup
        loadUrlAndWait(POPUP_URL);
        waitForText(mStringHelper.POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.POPUP_MESSAGE), true, "Popup blocker is displayed");

        // Wait for the popup to be shown.
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");

        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(mStringHelper.POPUP_ALLOW);
        waitForTextDismissed(mStringHelper.POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.POPUP_MESSAGE), false, "Popup blocker is hidden when popup allowed");

        try {
            final JSONObject data = new JSONObject(tabEventExpecter.blockForEventData());

            // Check to make sure the popup window was opened.
            mAsserter.is("data:text/plain;charset=utf-8,a", data.getString("uri"), "Checking popup URL");

            // Close the popup window.
            closeTab(data.getInt("tabID"));

        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting event data", e.toString());
        }
        tabEventExpecter.unregisterListener();

        // Load page with popup
        loadUrlAndWait(POPUP_URL);
        waitForText(mStringHelper.POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.POPUP_MESSAGE), true, "Popup blocker is displayed");

        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(mStringHelper.POPUP_DENY);
        waitForTextDismissed(mStringHelper.POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(mStringHelper.POPUP_MESSAGE), false, "Popup blocker is hidden when popup denied");

        // Check that we're on the same page to verify that the popup was not shown.
        verifyUrl(POPUP_URL);

        try {
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", "dom.disable_open_during_load");
            jsonPref.put("type", "bool");
            jsonPref.put("value", false);
            setPreferenceAndWaitForChange(jsonPref);
        } catch (JSONException e) {
            mAsserter.ok(false, "exception setting preference", e.toString());
        }
    }

    // wait for a CheckBox view that is clickable
    private void waitForCheckBox() {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                for (CheckBox view : mSolo.getCurrentViews(CheckBox.class)) {
                    // checking isClickable alone is not sufficient --
                    // intermittent "cannot click" errors persist unless
                    // additional checks are used
                    if (view.isClickable() &&
                        view.getVisibility() == View.VISIBLE &&
                        view.getWidth() > 0 &&
                        view.getHeight() > 0) {
                        return true;
                    }
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }

    // wait until the specified text is *not* displayed
    private void waitForTextDismissed(final String text) {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return !mSolo.searchText(text);
            }
        }, MAX_WAIT_MS);
    }
}
