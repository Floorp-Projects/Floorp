/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.gecko.tests;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;

import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;


public class testAccessibleCarets extends JavascriptTest {
    private static final String LOGTAG = "testAccessibleCarets";
    private static final String TAB_CHANGE_EVENT = "testAccessibleCarets:TabChange";

    private final TabsListener tabsListener;


    public testAccessibleCarets() {
        super("testAccessibleCarets.js");

        tabsListener = new TabsListener();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        Tabs.registerOnTabsChangedListener(tabsListener);
    }

    @Override
    public void tearDown() throws Exception {
        Tabs.unregisterOnTabsChangedListener(tabsListener);

        super.tearDown();
    }

    @Override
    public void testJavascript() throws Exception {
        // This feature is currently only available in Nightly.
        if (!AppConstants.NIGHTLY_BUILD) {
            mAsserter.dumpLog(LOGTAG + " is disabled on non-Nightly builds: returning");
            return;
        }
        super.testJavascript();
    }

    /**
     * Observes tab change events to broadcast to the test script.
     */
    private class TabsListener implements Tabs.OnTabsChangedListener {
        @Override
        public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
            switch (msg) {
                case STOP:
                    final JSONObject args = new JSONObject();
                    try {
                        args.put("tabId", tab.getId());
                        args.put("event", msg.toString());
                    } catch (JSONException e) {
                        Log.e(LOGTAG, "Error building JSON arguments for " + TAB_CHANGE_EVENT, e);
                        return;
                    }
                    mActions.sendGeckoEvent(TAB_CHANGE_EVENT, args.toString());
                    break;
            }
        }
    }
}
