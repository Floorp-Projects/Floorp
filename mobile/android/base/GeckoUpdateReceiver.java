/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.updater.UpdateServiceHelper;

import android.content.*;

public class GeckoUpdateReceiver
    extends BroadcastReceiver
{
    @Override
    public void onReceive(Context context, Intent intent) {
        if (UpdateServiceHelper.ACTION_CHECK_UPDATE_RESULT.equals(intent.getAction())) {
            String result = intent.getStringExtra("result");
            if (GeckoApp.mAppContext != null && result != null) {
                GeckoApp.mAppContext.notifyCheckUpdateResult(result);
            }
        }
    }
}
