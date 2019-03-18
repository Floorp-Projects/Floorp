/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.annotation.TargetApi;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.CheckResult;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.mozilla.gecko.mozglue.SafeIntent;

import java.net.URISyntaxException;
import java.util.HashMap;
import java.util.Locale;
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
     * with the key -&gt; value format: env# -&gt; ENV_VAR=VALUE, where # is an integer starting at 0.
     *
     * @return A Map of environment variable name to value, e.g. ENV_VAR -&gt; VALUE
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

    /**
     * Gets whether or not we're in automation from the passed in environment variables.
     *
     * We need to read environment variables from the intent string
     * extra because environment variables from our test harness aren't set
     * until Gecko is loaded, and we need to know this before then.
     *
     * The return value of this method should be used early since other
     * initialization may depend on its results.
     */
    @CheckResult
    public static boolean getIsInAutomationFromEnvironment(final SafeIntent intent) {
        final HashMap<String, String> envVars = IntentUtils.getEnvVarMap(intent);
        return !TextUtils.isEmpty(envVars.get(IntentUtils.ENV_VAR_IN_AUTOMATION));
    }

    /**
     * Return a Uri instance which is equivalent to uri,
     * but with a guaranteed-lowercase scheme as if the API level 16 method
     * Uri.normalizeScheme had been called.
     *
     * @param uri The URI string to normalize.
     * @return The corresponding normalized Uri.
     */
    private static Uri normalizeUriScheme(final Uri uri) {
        final String scheme = uri.getScheme();
        final String lower  = scheme.toLowerCase(Locale.US);
        if (lower.equals(scheme)) {
            return uri;
        }

        // Otherwise, return a new URI with a normalized scheme.
        return uri.buildUpon().scheme(lower).build();
    }


    /**
     * Return a normalized Uri instance that corresponds to the given URI string
     * with cross-API-level compatibility.
     *
     * @param aUri The URI string to normalize.
     * @return The corresponding normalized Uri.
     */
    public static Uri normalizeUri(final String aUri) {
        final Uri normUri = normalizeUriScheme(
            aUri.indexOf(':') >= 0
            ? Uri.parse(aUri)
            : new Uri.Builder().scheme(aUri).build());
        return normUri;
    }

    public static boolean isUriSafeForScheme(final String aUri) {
        return isUriSafeForScheme(normalizeUri(aUri));
    }

    /**
     * Verify whether the given URI is considered safe to load in respect to
     * its scheme.
     * Unsafe URIs should be blocked from further handling.
     *
     * @param aUri The URI instance to test.
     * @return Whether the provided URI is considered safe in respect to its
     *         scheme.
     */
    public static boolean isUriSafeForScheme(final Uri aUri) {
        final String scheme = aUri.getScheme();
        if ("tel".equals(scheme) || "sms".equals(scheme)) {
            // Bug 794034 - We don't want to pass MWI or USSD codes to the
            // dialer, and ensure the Uri class doesn't parse a URI
            // containing a fragment ('#')
            final String number = aUri.getSchemeSpecificPart();
            if (number.contains("#") || number.contains("*") ||
                aUri.getFragment() != null) {
                return false;
            }
        }

        if (("intent".equals(scheme) || "android-app".equals(scheme))) {
            // Bug 1356893 - Rject intents with file data schemes.
            return getSafeIntent(aUri) != null;
        }

        return true;
    }

    /**
     * Create a safe intent for the given URI.
     * Intents with file data schemes are considered unsafe.
     *
     * @param aUri The URI for the intent.
     * @return A safe intent for the given URI or null if URI is considered
     *         unsafe.
     */
    public static Intent getSafeIntent(final Uri aUri) {
        final Intent intent;
        try {
            intent = Intent.parseUri(aUri.toString(), 0);
        } catch (final URISyntaxException e) {
            return null;
        }

        final Uri data = intent.getData();
        if (data != null &&
            "file".equals(normalizeUriScheme(data).getScheme())) {
            return null;
        }

        // Only open applications which can accept arbitrary data from a browser.
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        // Prevent site from explicitly opening our internal activities,
        // which can leak data.
        intent.setComponent(null);
        nullIntentSelector(intent);

        return intent;
    }

    // We create a separate method to better encapsulate the @TargetApi use.
    @TargetApi(15)
    private static void nullIntentSelector(final Intent intent) {
        intent.setSelector(null);
    }

}
