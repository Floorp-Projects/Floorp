/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class GeckoService extends Service {
    private static final String LOGTAG = "GeckoService";

    @Override
    public void onCreate() {
        GeckoAppShell.ensureCrashHandling();
        GeckoAppShell.setApplicationContext(getApplicationContext());

        super.onCreate();

        GeckoThread.ensureInit(/* args */ null, /* action */ null);
        GeckoThread.launch();
    }

    @Override
    public void onDestroy() {
        // TODO: Make sure Gecko is in a somewhat idle state
        // because our process could be killed at anytime.

        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
