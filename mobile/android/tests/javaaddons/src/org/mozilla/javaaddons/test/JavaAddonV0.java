/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.javaaddons.test;

import android.os.Handler;
import android.os.Message;
import android.util.Log;

import java.util.Map;

public class JavaAddonV0 implements Handler.Callback {
    public JavaAddonV0(Map<String, Handler.Callback> callbacks) {
        callbacks.put("JavaAddon:V0", this);
    }

    @Override
    public boolean handleMessage(Message message) {
        Log.i("JavaAddon", "handleMessage " + message.toString());
        return true;
    }
}
