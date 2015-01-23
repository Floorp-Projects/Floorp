package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

import android.util.Log;

import org.json.JSONObject;


public class testInputSelections extends UITest {

    public void testInputSelections() {
        GeckoHelper.blockForReady();

        Actions.EventExpecter robocopTestExpecter =
            getActions().expectGeckoEvent("Robocop:testInputSelections");
        final String url = "chrome://roboextender/content/testInputSelections.html";
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(url);

        while (!test(robocopTestExpecter)) {
            // do nothing
        }

        robocopTestExpecter.unregisterListener();
    }

    private boolean test(Actions.EventExpecter expecter) {
        final JSONObject eventData;
        try {
            eventData = new JSONObject(expecter.blockForEventData());
        } catch(Exception ex) {
            // Log and ignore
            getAsserter().ok(false, "JS Test", "Error decoding data " + ex);
            return false;
        }

        if (eventData.has("result")) {
            getAsserter().ok(eventData.optBoolean("result"), "JS Test", eventData.optString("msg"));
        } else if (eventData.has("todo")) {
            getAsserter().todo(eventData.optBoolean("todo"), "JS TODO", eventData.optString("msg"));
        }

        EventDispatcher.sendResponse(eventData, new JSONObject());
        return eventData.optBoolean("done", false);
    }
}
