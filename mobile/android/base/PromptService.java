/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoEventResponder;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.content.Context;

import java.util.concurrent.ConcurrentLinkedQueue;

public class PromptService implements GeckoEventResponder {
    private static final String LOGTAG = "GeckoPromptService";

    private final ConcurrentLinkedQueue<String> mPromptQueue;
    private final Context mContext;

    public PromptService(Context context) {
        GeckoAppShell.getEventDispatcher().registerEventListener("Prompt:Show", this);
        GeckoAppShell.getEventDispatcher().registerEventListener("Prompt:ShowTop", this);
        mPromptQueue = new ConcurrentLinkedQueue<String>();
        mContext = context;
    }

    void destroy() {
        GeckoAppShell.getEventDispatcher().unregisterEventListener("Prompt:Show", this);
        GeckoAppShell.getEventDispatcher().unregisterEventListener("Prompt:ShowTop", this);
    }

    public void show(final String aTitle, final String aText, final Prompt.PromptListItem[] aMenuList,
                     final boolean aMultipleSelection, final Prompt.PromptCallback callback) {
        // The dialog must be created on the UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Prompt p;
                if (callback != null) {
                    p = new Prompt(mContext, callback);
                } else {
                    p = new Prompt(mContext, mPromptQueue);
                }
                p.show(aTitle, aText, aMenuList, aMultipleSelection);
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
                boolean isAsync = message.optBoolean("async");
                Prompt p;
                if (isAsync) {
                    p = new Prompt(mContext, new Prompt.PromptCallback() {
                        public void onPromptFinished(String jsonResult) {
                            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Prompt:Reply", jsonResult));
                        }
                    });
                } else {
                    p = new Prompt(mContext, mPromptQueue);
                }
                p.show(message);
            }
        });
    }

    // GeckoEventResponder implementation
    @Override
    public String getResponse(final JSONObject origMessage) {
        if (origMessage.optBoolean("async")) {
            return "";
        }

        // we only handle one kind of message in handleMessage, and this is the
        // response we provide for that message
        String result;
        while (null == (result = mPromptQueue.poll())) {
            GeckoAppShell.processNextNativeEvent(true);
        }
        return result;
    }
}
