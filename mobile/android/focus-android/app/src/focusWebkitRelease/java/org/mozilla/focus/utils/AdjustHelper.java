/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;
import android.text.TextUtils;

import com.adjust.sdk.Adjust;
import com.adjust.sdk.AdjustConfig;

import org.mozilla.focus.BuildConfig;

public class AdjustHelper {
    public static void setupAdjustIfNeeded(Context context) {
        // RELEASE: Enable Adjust - This class has different implementations for all build types.

        //noinspection ConstantConditions
        if (TextUtils.isEmpty(BuildConfig.ADJUST_TOKEN)) {
            throw new IllegalStateException("No adjust token defined for release build");
        }

        AdjustConfig config = new AdjustConfig(context, BuildConfig.ADJUST_TOKEN, AdjustConfig.ENVIRONMENT_PRODUCTION);
        Adjust.onCreate(config);
    }
}
