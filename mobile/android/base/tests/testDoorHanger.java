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
        String GEO_URL = getAbsoluteUrl(StringHelper.ROBOCOP_GEOLOCATION_URL);
        String BLANK_URL = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String OFFLINE_STORAGE_URL = getAbsoluteUrl(StringHelper.ROBOCOP_OFFLINE_STORAGE_URL);
        String LOGIN_URL = getAbsoluteUrl(StringHelper.ROBOCOP_LOGIN_URL);

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
        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(GEO_ALLOW);
        waitForTextDismissed(GEO_MESSAGE);
        mAsserter.is(mSolo.searchText(GEO_MESSAGE), false, "Geolocation doorhanger has been hidden when allowing share");

        // Re-trigger geolocation notification
        inputAndLoadUrl(GEO_URL);
        waitForText(GEO_MESSAGE);

        // Test "Don't share" button hides the notification
        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(GEO_DENY);
        waitForTextDismissed(GEO_MESSAGE);
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
        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(OFFLINE_DENY);
        waitForTextDismissed(OFFLINE_MESSAGE);
        mAsserter.is(mSolo.searchText(OFFLINE_MESSAGE), false, "Offline storage doorhanger notification is hidden when denying storage");

        // Load offline storage page
        inputAndLoadUrl(OFFLINE_STORAGE_URL);
        waitForText(OFFLINE_MESSAGE);

        // Test doorhanger dismissed when tapping "Allow" and is not displayed again
        mSolo.clickOnButton(OFFLINE_ALLOW);
        waitForTextDismissed(OFFLINE_MESSAGE);
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
        waitForTextDismissed(LOGIN_MESSAGE);
        mAsserter.is(mSolo.searchText(LOGIN_MESSAGE), false, "Login doorhanger notification is hidden when denying saving password");

        // Load login page
        inputAndLoadUrl(LOGIN_URL);
        waitForText(LOGIN_MESSAGE);

        // Test doorhanger is dismissed when tapping "Save" and is no longer triggered
        mSolo.clickOnButton(LOGIN_ALLOW);
        waitForTextDismissed(LOGIN_MESSAGE);
        mAsserter.is(mSolo.searchText(LOGIN_MESSAGE), false, "Login doorhanger notification is hidden when allowing saving password");

        testPopupBlocking();
    }

    private void testPopupBlocking() {
        String POPUP_URL = getAbsoluteUrl(StringHelper.ROBOCOP_POPUP_URL);
        String POPUP_MESSAGE = "prevented this site from opening";
        String POPUP_ALLOW = "Show";
        String POPUP_DENY = "Don't show";

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
        inputAndLoadUrl(POPUP_URL);
        waitForText(POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(POPUP_MESSAGE), true, "Popup blocker is displayed");

        // Wait for the popup to be shown.
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");

        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(POPUP_ALLOW);
        waitForTextDismissed(POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(POPUP_MESSAGE), false, "Popup blocker is hidden when popup allowed");

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
        inputAndLoadUrl(POPUP_URL);
        waitForText(POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(POPUP_MESSAGE), true, "Popup blocker is displayed");

        waitForCheckBox();
        mSolo.clickOnCheckBox(0);
        mSolo.clickOnButton(POPUP_DENY);
        waitForTextDismissed(POPUP_MESSAGE);
        mAsserter.is(mSolo.searchText(POPUP_MESSAGE), false, "Popup blocker is hidden when popup denied");

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
