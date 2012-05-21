/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;

import android.content.Context;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.util.Log;

class ConfirmPreference extends DialogPreference {
    private static final String LOGTAG = "GeckoConfirmPreference";

    private String mAction = null;
    private Context mContext = null;
    public ConfirmPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAction = attrs.getAttributeValue(null, "action");
        mContext = context;
    }
    public ConfirmPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mAction = attrs.getAttributeValue(null, "action");
        mContext = context;
    }
    protected void onDialogClosed(boolean positiveResult) {
        if (!positiveResult)
            return;
        if ("clear_history".equalsIgnoreCase(mAction)) {
            GeckoAppShell.getHandler().post(new Runnable(){
                public void run() {
                    BrowserDB.clearHistory(mContext.getContentResolver());
                    GeckoApp.mAppContext.mFavicons.clearFavicons();
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("browser:purge-session-history", null));
                    GeckoApp.mAppContext.handleClearHistory();
                }
            });
        } else if ("clear_private_data".equalsIgnoreCase(mAction)) {
            GeckoAppShell.getHandler().post(new Runnable(){
                public void run() {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Sanitize:ClearAll", null));
                }
            });
        }
        Log.i(LOGTAG, "action: " + mAction);
    }
}
