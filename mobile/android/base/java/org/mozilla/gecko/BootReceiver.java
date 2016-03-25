/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.mozilla.gecko.feeds.FeedService;

/**
 * This broadcast receiver receives ACTION_BOOT_COMPLETED broadcasts and starts components that should
 * run after the device has booted.
 */
public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || !intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED)) {
            return; // This is not the broadcast you are looking for.
        }

        FeedService.setup(context);
    }
}
