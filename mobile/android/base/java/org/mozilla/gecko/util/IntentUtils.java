/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Intent;
import android.support.annotation.NonNull;

import java.util.HashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Utilities for Intents.
 */
public class IntentUtils {
    private static final String ENV_VAR_REGEX = "(.+)=(.*)";

    private IntentUtils() {}

    /**
     * Returns a list of environment variables and their values. These are parsed from an Intent extra
     * with the key -> value format:
     *   env# -> ENV_VAR=VALUE
     *
     * # in env# is expected to be increasing from 0.
     *
     * @return A Map of environment variable name to value, e.g. ENV_VAR -> VALUE
     */
    public static HashMap<String, String> getEnvVarMap(@NonNull final Intent intent) {
        final HashMap<String, String> out = new HashMap<>();
        final Pattern envVarPattern = Pattern.compile(ENV_VAR_REGEX);
        Matcher matcher = null;

        String envValue = intent.getStringExtra("env0");
        int i = 1;
        while (envValue != null) {
            // Optimization to re-use matcher rather than creating new
            // objects because this is used in the startup path.
            if (matcher == null) {
                matcher = envVarPattern.matcher(envValue);
            } else {
                matcher.reset(envValue);
            }

            if (matcher.matches()) {
                final String envVarName = matcher.group(1);
                final String envVarValue = matcher.group(2);
                out.put(envVarName, envVarValue);
            }
            envValue = intent.getStringExtra("env" + i);
            i += 1;
        }
        return out;
    }
}
