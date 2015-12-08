/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.b2gdroid;

import android.app.IntentService;
import android.content.Intent;
import android.os.Bundle;

import org.mozilla.gecko.GeckoAppShell;

public class HeadlessSmsSendService extends IntentService {
    private final static String ACTION_RESPOND_VIA_MESSAGE = "android.intent.action.RESPOND_VIA_MESSAGE";

    public HeadlessSmsSendService() {
        super("HeadlessSmsSendService");
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (!ACTION_RESPOND_VIA_MESSAGE.equals(intent.getAction())) {
            return;
        }

        Bundle extras = intent.getExtras();
        if (extras == null) {
            return;
        }

        String recipient = intent.getData().getPath();
        String message = extras.getString(Intent.EXTRA_TEXT);

        if (recipient.length() == 0 || message.length() == 0) {
            return;
        }

        GeckoAppShell.sendMessage(recipient, message, 0, /* shouldNotify */ false);
    }
}
