/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Actions.EventExpecter;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.GeckoThread.LaunchState;
import org.mozilla.gecko.tests.UITestContext;

import android.app.Activity;

/**
 * Provides helper functions for accessing the underlying Gecko engine.
 */
public final class GeckoHelper {
    private static Activity sActivity;
    private static Actions sActions;

    private GeckoHelper() { /* To disallow instantiation. */ }

    public static void init(final UITestContext context) {
        sActivity = context.getActivity();
        sActions = context.getActions();
    }

    public static void blockForReady() {
        final EventExpecter geckoReady = sActions.expectGeckoEvent("Gecko:Ready");

        final boolean isReady = GeckoThread.checkLaunchState(LaunchState.GeckoRunning);
        if (!isReady) {
            geckoReady.blockForEvent();
        }

        geckoReady.unregisterListener();
    }
}
