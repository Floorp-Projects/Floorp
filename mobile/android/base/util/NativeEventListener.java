/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.mozglue.RobocopTarget;

@RobocopTarget
public interface NativeEventListener {
    /**
     * Handles a message sent from Gecko.
     *
     * @param event    The name of the event being sent.
     * @param message  The message data.
     * @param callback The callback interface for this message. A callback is provided only if the
     *                 originating sendMessageToJava call included a callback argument; otherwise,
     *                 callback will be null. All listeners for a given event are given the same
     *                 callback object, and exactly one listener must handle the callback.
     */
    void handleMessage(String event, NativeJSObject message, EventCallback callback);
}
