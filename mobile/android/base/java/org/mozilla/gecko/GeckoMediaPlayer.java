/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONObject;
import org.mozilla.gecko.util.EventCallback;

/**
 * Wrapper for MediaRouter types supported by Android, such as Chromecast, Miracast, etc.
 */
interface GeckoMediaPlayer {
    /**
     * Can return null.
     */
    JSONObject toJSON();
    void load(String title, String url, String type, EventCallback callback);
    void play(EventCallback callback);
    void pause(EventCallback callback);
    void stop(EventCallback callback);
    void start(EventCallback callback);
    void end(EventCallback callback);
    void mirror(EventCallback callback);
    void message(String message, EventCallback callback);
}
