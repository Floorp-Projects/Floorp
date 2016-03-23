/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.Intent;
import android.util.Log;

import org.mozilla.gecko.db.BrowserDB;

/**
 * Interface for actions run by FeedService.
 */
public abstract class FeedAction {
    public static final boolean DEBUG_LOG = false;

    /**
     * Perform this action.
     *
     * @param browserDB database instance to perform the action.
     * @param intent used to start the service.
     */
    public abstract void perform(BrowserDB browserDB, Intent intent);

    /**
     * Does this action require an active network connection?
     */
    public abstract boolean requiresNetwork();

    /**
     * Should this action only run if the preference is enabled?
     */
    public abstract boolean requiresPreferenceEnabled();

    /**
     * This method will swallow all log messages to avoid logging potential personal information.
     *
     * For debugging purposes set {@code DEBUG_LOG} to true.
     */
    public void log(String message) {
        if (DEBUG_LOG) {
            Log.d("Gecko" + getClass().getSimpleName(), message);
        }
    }

    /**
     * This method will swallow all log messages to avoid logging potential personal information.
     *
     * For debugging purposes set {@code DEBUG_LOG} to true.
     */
    public void log(String message, Throwable throwable) {
        if (DEBUG_LOG) {
            Log.d("Gecko" + getClass().getSimpleName(), message, throwable);
        }
    }
}
