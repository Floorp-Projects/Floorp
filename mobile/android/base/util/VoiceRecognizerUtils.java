/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.Intent;
import android.speech.RecognizerIntent;

public class VoiceRecognizerUtils {
    public static boolean supportsVoiceRecognizer(Context context, String prompt) {
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
}
