package org.mozilla.gecko.tests;

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
        String GEO_URL = getAbsoluteUrl("/robocop/robocop_geolocation.html");
        String BLANK_URL = getAbsoluteUrl("/robocop/robocop_blank_01.html");
        String OFFLINE_STORAGE_URL = getAbsoluteUrl("/robocop/robocop_offline_storage.html");
        String LOGIN_URL = getAbsoluteUrl("/robocop/robocop_login.html");

        // Strings used in doorhanger messages and buttons
        String GEO_MESSAGE = "Share your location with";
        String GEO_ALLOW = "Share";
        String GEO_DENY = "Don't share";

        String OFFLINE_MESSAGE = "to store data on your device for offline use";
        String OFFLINE_ALLOW = "Allow";
        String OFFLINE_DENY = "Don't allow";

        String LOGIN_MESSAGE = "Save password";
        String LOGIN_ALLOW = "Save";
        String LOGIN_DENY = "Don't save";

        blockForGeckoReady();

        // Test geolocation notification
        inputAndLoadUrl(GEO_URL);
        waitForText(GEO_MESSAGE);
        mAsserter.is(mSolo.searchText(GEO_MESSAGE), true, "Geolocation doorhanger has been displayed");

        // Test "Share" button hides the notification
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(GEO_ALLOW);
        mAsserter.is(mSolo.searchText(GEO_MESSAGE), false, "Geolocation doorhanger has been hidden when allowing share");

        // Re-trigger geolocation notification
        inputAndLoadUrl(GEO_URL);
        waitForText(GEO_MESSAGE);

        // Test "Don't share" button hides the notification
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(GEO_DENY);
        mAsserter.is(mSolo.searchText(GEO_MESSAGE), false, "Geolocation doorhanger has been hidden when denying share");

        /* FIXME: disabled on fig - bug 880060 (for some reason this fails because of some raciness)
        // Re-trigger geolocation notification
        inputAndLoadUrl(GEO_URL);
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
        inputAndLoadUrl(OFFLINE_STORAGE_URL);
        waitForText(OFFLINE_MESSAGE);

        // Test doorhanger dismissed when tapping "Don't share"
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(OFFLINE_DENY);
        mAsserter.is(mSolo.searchText(OFFLINE_MESSAGE), false, "Offline storage doorhanger notification is hidden when denying storage");

        // Load offline storage page
        inputAndLoadUrl(OFFLINE_STORAGE_URL);
        waitForText(OFFLINE_MESSAGE);

        // Test doorhanger dismissed when tapping "Allow" and is not displayed again
        mSolo.clickOnButton(OFFLINE_ALLOW);
        mAsserter.is(mSolo.searchText(OFFLINE_MESSAGE), false, "Offline storage doorhanger notification is hidden when allowing storage");
        inputAndLoadUrl(OFFLINE_STORAGE_URL);
        mAsserter.is(mSolo.searchText(OFFLINE_MESSAGE), false, "Offline storage doorhanger is no longer triggered");

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


        // Load login page
        inputAndLoadUrl(LOGIN_URL);
        waitForText(LOGIN_MESSAGE);

        // Test doorhanger is dismissed when tapping "Don't save"
        mSolo.clickOnButton(LOGIN_DENY);
        mAsserter.is(mSolo.searchText(LOGIN_MESSAGE), false, "Login doorhanger notification is hidden when denying saving password");

        // Load login page
        inputAndLoadUrl(LOGIN_URL);
        waitForText(LOGIN_MESSAGE);

        // Test doorhanger is dismissed when tapping "Save" and is no longer triggered
        mSolo.clickOnButton(LOGIN_ALLOW);
        mAsserter.is(mSolo.searchText(LOGIN_MESSAGE), false, "Login doorhanger notification is hidden when allowing saving password");

        // Reload the page and check that there is no doorhanger displayed
        inputAndLoadUrl(LOGIN_URL);
        mAsserter.is(mSolo.searchText(LOGIN_MESSAGE), false, "Login doorhanger is not re-triggered");
    }
}
