/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.util.Log;
import android.view.View;
import org.json.JSONObject;

class TextSelection implements GeckoEventListener {
    private static final String LOGTAG = "GeckoTextSelection";

    private final TextSelectionHandle mStartHandle;
    private final TextSelectionHandle mEndHandle;

    TextSelection(TextSelectionHandle startHandle, TextSelectionHandle endHandle) {
        mStartHandle = startHandle;
        mEndHandle = endHandle;

        GeckoAppShell.registerGeckoEventListener("TextSelection:ShowHandles", this);
        GeckoAppShell.registerGeckoEventListener("TextSelection:HideHandles", this);
        GeckoAppShell.registerGeckoEventListener("TextSelection:PositionHandles", this);
    }

    void destroy() {
        GeckoAppShell.unregisterGeckoEventListener("TextSelection:ShowHandles", this);
        GeckoAppShell.unregisterGeckoEventListener("TextSelection:HideHandles", this);
        GeckoAppShell.unregisterGeckoEventListener("TextSelection:PositionHandles", this);
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("TextSelection:ShowHandles")) {
                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        mStartHandle.setVisibility(View.VISIBLE);
                        mEndHandle.setVisibility(View.VISIBLE);
                    }
                });
            } else if (event.equals("TextSelection:HideHandles")) {
                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        mStartHandle.setVisibility(View.GONE);
                        mEndHandle.setVisibility(View.GONE);
                    }
                });
            } else if (event.equals("TextSelection:PositionHandles")) {
                final int startLeft = message.getInt("startLeft");
                final int startTop = message.getInt("startTop");
                final int endLeft = message.getInt("endLeft");
                final int endTop = message.getInt("endTop");

                GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                    public void run() {
                        mStartHandle.positionFromGecko(startLeft, startTop);
                        mEndHandle.positionFromGecko(endLeft, endTop);
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }
}
