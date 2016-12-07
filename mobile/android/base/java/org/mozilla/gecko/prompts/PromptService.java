/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.util.Log;

public class PromptService implements BundleEventListener {
    private static final String LOGTAG = "GeckoPromptService";

    private final Context mContext;

    public PromptService(Context context) {
        GeckoApp.getEventDispatcher().registerUiThreadListener(this,
            "Prompt:Show",
            "Prompt:ShowTop");
        mContext = context;
    }

    public void destroy() {
        GeckoApp.getEventDispatcher().unregisterUiThreadListener(this,
            "Prompt:Show",
            "Prompt:ShowTop");
    }

    // BundleEventListener implementation
    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        Prompt p;
        p = new Prompt(mContext, new Prompt.PromptCallback() {
            @Override
            public void onPromptFinished(final GeckoBundle result) {
                callback.sendSuccess(result);
            }
        });
        p.show(message);
    }
}
