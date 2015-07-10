/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.InputOptionsUtils;

/**
 * Supports the DevTools WiFi debugging authentication flow by invoking a QR decoder.
 */
public class DevToolsAuthHelper {

    public static void scan(Context context, final EventCallback callback) {
        final Intent intent = InputOptionsUtils.createQRCodeReaderIntent();

        intent.putExtra("PROMPT_MESSAGE", context.getString(R.string.devtools_auth_scan_header));

        Activity activity = GeckoAppShell.getGeckoInterface().getActivity();
        ActivityHandlerHelper.startIntentForActivity(activity, intent, new ActivityResultHandler() {
            @Override
            public void onActivityResult(int resultCode, Intent intent) {
                if (resultCode == Activity.RESULT_OK) {
                    String text = intent.getStringExtra("SCAN_RESULT");
                    callback.sendSuccess(text);
                } else {
                    callback.sendError(resultCode);
                }
            }
        });
    }

}
