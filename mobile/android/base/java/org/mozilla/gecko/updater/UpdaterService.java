/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;
import android.support.v4.app.JobIntentService;

import org.mozilla.gecko.CrashHandler;

public abstract class UpdaterService extends JobIntentService {
    private static CrashHandler crashHandler;
    protected Updater updater;
    protected UpdatesPrefs prefs;

    @Override
    public void onCreate() {
        initCrashHandler(getApplicationContext());

        super.onCreate();

        prefs = new UpdatesPrefs(this);
        updater = new Updater(this, prefs, UpdateServiceHelper.BUFSIZE, UpdateServiceHelper.NOTIFICATION_ID);
    }

    @Override
    protected abstract void onHandleWork(@NonNull Intent intent);

    @Override
    public void onDestroy() {
        super.onDestroy();
        updater.finish();
        crashHandler.unregister();
    }

    private void initCrashHandler(@NonNull final Context appContext) {
        // Will only need one
        if (crashHandler == null) {
            synchronized (this) {
                if (crashHandler == null) {
                    crashHandler = CrashHandler.createDefaultCrashHandler(appContext);
                }
            }
        }
    }
}
