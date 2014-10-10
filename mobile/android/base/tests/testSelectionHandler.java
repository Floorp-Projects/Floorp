package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

import android.util.Log;

import org.json.JSONObject;


public class testSelectionHandler extends UITest {

    public void testSelectionHandler() {
        GeckoHelper.blockForReady();

        Actions.EventExpecter robocopTestExpecter = getActions().expectGeckoEvent("Robocop:testSelectionHandler");
        final String url = "chrome://roboextender/content/testSelectionHandler.html";
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(StringHelper.ROBOCOP_SELECTION_HANDLER_TITLE, url);

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
        }

        EventDispatcher.sendResponse(eventData, new JSONObject());
        return eventData.optBoolean("done", false);
    }
}
