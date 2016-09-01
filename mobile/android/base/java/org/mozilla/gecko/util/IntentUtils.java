/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import org.mozilla.gecko.mozglue.SafeIntent;

import java.util.HashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Utilities for Intents.
 */
public class IntentUtils {
    public static final String ENV_VAR_IN_AUTOMATION = "MOZ_IN_AUTOMATION";

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
    public static HashMap<String, String> getEnvVarMap(@NonNull final SafeIntent intent) {
        // Optimization: get matcher for re-use. Pattern.matcher creates a new object every time so it'd be great
        // to avoid the unnecessary allocation, particularly because we expect to be called on the startup path.
        final Pattern envVarPattern = Pattern.compile(ENV_VAR_REGEX);
        final Matcher matcher = envVarPattern.matcher(""); // argument does not matter here.

        // This is expected to be an external intent so we should use SafeIntent to prevent crashing.
        final HashMap<String, String> out = new HashMap<>();
        int i = 0;
        while (true) {
            final String envKey = "env" + i;
            i += 1;
            if (!intent.hasExtra(envKey)) {
                break;
            }

            maybeAddEnvVarToEnvVarMap(out, intent, envKey, matcher);
        }
        return out;
    }

    /**
     * @param envVarMap the map to add the env var to
     * @param intent the intent from which to extract the env var
     * @param envKey the key at which the env var resides
     * @param envVarMatcher a matcher initialized with the env var pattern to extract
     */
    private static void maybeAddEnvVarToEnvVarMap(@NonNull final HashMap<String, String> envVarMap,
            @NonNull final SafeIntent intent, @NonNull final String envKey, @NonNull final Matcher envVarMatcher) {
        final String envValue = intent.getStringExtra(envKey);
        if (envValue == null) {
            return; // nothing to do here!
        }

        envVarMatcher.reset(envValue);
        if (envVarMatcher.matches()) {
            final String envVarName = envVarMatcher.group(1);
            final String envVarValue = envVarMatcher.group(2);
            envVarMap.put(envVarName, envVarValue);
        }
    }

    public static Bundle getBundleExtraSafe(final Intent intent, final String name) {
        return new SafeIntent(intent).getBundleExtra(name);
    }

    public static String getStringExtraSafe(final Intent intent, final String name) {
        return new SafeIntent(intent).getStringExtra(name);
    }

    public static boolean getBooleanExtraSafe(final Intent intent, final String name, final boolean defaultValue) {
        return new SafeIntent(intent).getBooleanExtra(name, defaultValue);
    }
}
