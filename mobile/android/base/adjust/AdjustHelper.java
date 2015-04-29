/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.adjust;

import android.content.Context;
import android.content.Intent;

import com.adjust.sdk.Adjust;
import com.adjust.sdk.AdjustConfig;
import com.adjust.sdk.AdjustReferrerReceiver;
import com.adjust.sdk.LogLevel;

public class AdjustHelper implements AdjustHelperInterface {
    public void onCreate(final Context context, final String maybeAppToken) {
        final String environment;
        final String appToken;
        final LogLevel logLevel;
        if (maybeAppToken != null) {
            environment = AdjustConfig.ENVIRONMENT_PRODUCTION;
            appToken = maybeAppToken;
            logLevel = LogLevel.WARN;
        } else {
            environment = AdjustConfig.ENVIRONMENT_SANDBOX;
            appToken = "ABCDEFGHIJKL";
            logLevel = LogLevel.VERBOSE;
        }
        AdjustConfig config = new AdjustConfig(context, appToken, environment);
        config.setLogLevel(logLevel);
        Adjust.onCreate(config);
    }

    public void onReceive(final Context context, final Intent intent) {
        new AdjustReferrerReceiver().onReceive(context, intent);
    }
}
