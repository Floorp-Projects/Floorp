/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.gecko.tests;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.GeckoEventListener;

import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;

import org.json.JSONException;
import org.json.JSONObject;

public class testSelectionCarets extends JavascriptTest implements GeckoEventListener {
    private static final String LOGTAG = "testSelectionCarets";

    private static final String LONGPRESS_EVENT = "testSelectionCarets:Longpress";
    private static final String TAB_CHANGE_EVENT = "testSelectionCarets:TabChange";

    private final TabsListener tabsListener;

    public testSelectionCarets() {
        super("testSelectionCarets.js");

        tabsListener = new TabsListener();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        Tabs.registerOnTabsChangedListener(tabsListener);
        EventDispatcher.getInstance().registerGeckoThreadListener(this, LONGPRESS_EVENT);
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

    @Override
    public void tearDown() throws Exception {
        Tabs.unregisterOnTabsChangedListener(tabsListener);
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this, LONGPRESS_EVENT);

        super.tearDown();
    }

    /**
     * The test script will request us to trigger Longpress AndroidGeckoEvents.
    */
    @Override
    public void handleMessage(String event, final JSONObject message) {
        switch(event) {
            case LONGPRESS_EVENT: {
                final long meTime = SystemClock.uptimeMillis();
                final int meX = Math.round(message.optInt("x", 0));
                final int meY = Math.round(message.optInt("y", 0));
                final MotionEvent motionEvent =
                    MotionEvent.obtain(meTime, meTime, MotionEvent.ACTION_DOWN, meX, meY, 0);

                final GeckoEvent geckoEvent = GeckoEvent.createLongPressEvent(motionEvent);
                GeckoAppShell.sendEventToGecko(geckoEvent);
                break;
            }
        }
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
                    final GeckoEvent event =
                        GeckoEvent.createBroadcastEvent(TAB_CHANGE_EVENT, args.toString());
                    GeckoAppShell.sendEventToGecko(event);
                    break;
            }
        }
    }
}
