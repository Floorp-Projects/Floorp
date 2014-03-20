/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * This BroadcastReceiver is registered in the AndroidManifest.xml.in file.
 *
 * <p>It listens for intents sent by synthesized APKs when the task has been ended.
 * e.g. when the user has swiped it out of the Recent Apps List.</p>
 *
 */
public class TaskKiller extends BroadcastReceiver {

    private static final String LOGTAG = "GeckoWebappTaskKiller";

    @Override
    public void onReceive(Context context, Intent intent) {
        String packageName = intent.getStringExtra("packageName");
        int slot = Allocator.getInstance(context).getIndexForApp(packageName);
        if (slot >= 0) {
            EventListener.killWebappSlot(context, slot);
        } else {
            Log.w(LOGTAG, "Asked to kill " + packageName + " but this runtime (" + context.getPackageName() + ") doesn't know about it.");
        }
    }

}
