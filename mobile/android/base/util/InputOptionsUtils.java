/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.Intent;
import android.speech.RecognizerIntent;
import org.mozilla.gecko.AppConstants.Versions;

public class InputOptionsUtils {
    public static boolean supportsVoiceRecognizer(Context context, String prompt) {
        if (Versions.preICS) {
            return false;
        }
        final Intent intent = createVoiceRecognizerIntent(prompt);
        return intent.resolveActivity(context.getPackageManager()) != null;
    }

    public static Intent createVoiceRecognizerIntent(String prompt) {
        final Intent intent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        intent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_WEB_SEARCH);
        intent.putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 1);
        intent.putExtra(RecognizerIntent.EXTRA_PROMPT, prompt);
        return intent;
    }

    public static boolean supportsIntent(Intent intent, Context context) {
        return intent.resolveActivity(context.getPackageManager()) != null;
    }

    public static boolean supportsQrCodeReader(Context context) {
        final Intent intent = createQRCodeReaderIntent();
        return supportsIntent(intent, context);
    }

    public static Intent createQRCodeReaderIntent() {
        // Bug 602818 enables QR code input if you have the particular app below installed in your device
        final String appPackage = "com.google.zxing.client.android";

        Intent intent = new Intent(appPackage + ".SCAN");
        intent.setPackage(appPackage);
        intent.putExtra("SCAN_MODE", "QR_CODE_MODE");
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return intent;
    }
}
