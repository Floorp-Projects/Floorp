/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.json.JSONObject;
import org.mozilla.gecko.annotation.RobocopTarget;

@RobocopTarget
public interface GeckoEventListener {
    void handleMessage(String event, JSONObject message);
}
