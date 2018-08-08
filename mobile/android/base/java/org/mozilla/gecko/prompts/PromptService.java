/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import android.content.Context;

public class PromptService implements BundleEventListener {
    private static final String LOGTAG = "GeckoPromptService";

    private final Context context;
    private final EventDispatcher dispatcher;
    private Prompt currentPrompt;
    private int currentOrientation;

    public PromptService(final Context context, final EventDispatcher dispatcher) {
        this.context = context;
        this.currentOrientation = context.getResources().getConfiguration().orientation;
        this.dispatcher = dispatcher;
        this.dispatcher.registerUiThreadListener(this,
            "Prompt:Show",
            "Prompt:ShowTop");
    }

    public void destroy() {
        dispatcher.unregisterUiThreadListener(this,
            "Prompt:Show",
            "Prompt:ShowTop");
    }

    // BundleEventListener implementation
    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        currentPrompt = new Prompt(context, new Prompt.PromptCallback() {
            @Override
            public void onPromptFinished(final GeckoBundle result) {
                callback.sendSuccess(result);
                currentPrompt = null;
            }
        });
        currentPrompt.show(message);
    }

    public void changePromptOrientation(int newOrientation) {
        if (currentPrompt != null && currentOrientation != newOrientation) {
            currentPrompt.resetLayout();
        }
        currentOrientation = newOrientation;
    }
}
