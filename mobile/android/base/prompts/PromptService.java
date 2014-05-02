/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.util.Log;

public class PromptService implements GeckoEventListener {
    private static final String LOGTAG = "GeckoPromptService";

    private final Context mContext;

    public PromptService(Context context) {
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Prompt:Show",
            "Prompt:ShowTop");
        mContext = context;
    }

    public void destroy() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Prompt:Show",
            "Prompt:ShowTop");
    }

    public void show(final String aTitle, final String aText, final PromptListItem[] aMenuList,
                     final int aChoiceMode, final Prompt.PromptCallback callback) {
        // The dialog must be created on the UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Prompt p;
                p = new Prompt(mContext, callback);
                p.show(aTitle, aText, aMenuList, aChoiceMode);
            }
        });
    }

    // GeckoEventListener implementation
    @Override
    public void handleMessage(String event, final JSONObject message) {
        // The dialog must be created on the UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Prompt p;
                p = new Prompt(mContext, new Prompt.PromptCallback() {
                    public void onPromptFinished(String jsonResult) {
                        try {
                            EventDispatcher.sendResponse(message, new JSONObject(jsonResult));
                        } catch(JSONException ex) {
                            Log.i(LOGTAG, "Error building json response", ex);
                        }
                    }
                });
                p.show(message);
            }
        });
    }
}
