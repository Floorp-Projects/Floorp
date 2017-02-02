/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.RobocopTarget;

@RobocopTarget
public interface BundleEventListener {
    /**
     * Handles a message sent from Gecko.
     *
     * @param event    The name of the event being sent.
     * @param message  The message data.
     * @param callback The callback interface for this message. A callback is provided
     *                 only if the originating call included a callback argument;
     *                 otherwise, callback will be null.
     */
    void handleMessage(String event, GeckoBundle message, EventCallback callback);
}
