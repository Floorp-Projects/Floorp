/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webview.matcher;

import android.content.SharedPreferences;
import android.net.Uri;
import android.os.StrictMode;
import android.preference.PreferenceManager;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.R;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

/**
 * Integration test for tracking protection, which tests that some real websites are/aren't
 * blocked under given circumstances.
 *
 * UrlMatcherTest tests the general correctness of our URL matching code, this test is intended
 * to verify that our disconnect lists are loaded correctly (and therefore are dependent on the
 * contents of our disconnect lists).
 *
 * This test also verifies that the entity lists (whitelists for specific domains) actually work
 */
@RunWith(RobolectricTestRunner.class)
@Config(constants = BuildConfig.class, packageName = "org.mozilla.focus")
public class DisconnectTest {

    @After
    public void cleanup() {
        // Reset strict mode: for every test, Robolectric will create FocusApplication again.
        // FocusApplication expects strict mode to be disabled (since it loads some preferences from disk),
        // before enabling it itself. If we run multiple tests, strict mode will stay enabled
        // and FocusApplication crashes during initialisation for the second test.
        // This applies across multiple Test classes, e.g. DisconnectTest can cause
        // TrackingProtectionWebViewCLientTest to fail, unless it clears StrictMode first.
        // (FocusApplicaiton is initialised before @Before methods are run.)
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder().build());
    }

    // IMPORTANT NOTE - IF RUNNING TESTS USING ANDROID STUDIO:
    // Read the following for a guide on how to get AS to correctly load resources, if you don't
    // do this you will get Resource Loading exceptions:
    // http://robolectric.org/getting-started/#note-for-linux-and-mac-users
    @Test
    public void matches() throws Exception {
        final UrlMatcher matcher = UrlMatcher.loadMatcher(RuntimeEnvironment.application, R.raw.blocklist, new int[] { R.raw.google_mapping }, R.raw.entitylist);


        // Enable everything
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(RuntimeEnvironment.application);
        prefs.edit()
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_ads), true)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_analytics), true)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_other), true)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_social), true)
                .apply();

        // We check that our google_mapping was loaded correctly. We do these checks per-category, so we have:
        // ads:
        assertTrue(matcher.matches(Uri.parse("http://admeld.com/foobar"), Uri.parse("http://mozilla.org")));
        // And we check that the entitylist unblocks this on google properties:
        assertFalse(matcher.matches(Uri.parse("http://admeld.com/foobar"), Uri.parse("http://google.com")));
        // analytics:
        assertTrue(matcher.matches(Uri.parse("http://google-analytics.com/foobar"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://google-analytics.com/foobar"), Uri.parse("http://google.com")));
        // social:
        assertTrue(matcher.matches(Uri.parse("http://plus.google.com/something"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://plus.google.com/something"), Uri.parse("http://google.com")));

        // Facebook is special in that we move it from "Disconnect" into social:
        assertTrue(matcher.matches(Uri.parse("http://facebook.fr"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://facebook.fr"), Uri.parse("http://facebook.com")));

        // Now disable social, and check that only social sites have changed:
        prefs.edit()
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_ads), true)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_analytics), true)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_other), true)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_social), false)
                .apply();

        // ads:
        assertTrue(matcher.matches(Uri.parse("http://admeld.com/foobar"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://admeld.com/foobar"), Uri.parse("http://google.com")));
        // analytics:
        assertTrue(matcher.matches(Uri.parse("http://google-analytics.com/foobar"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://google-analytics.com/foobar"), Uri.parse("http://google.com")));
        // social:
        assertFalse(matcher.matches(Uri.parse("http://plus.google.com/something"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://plus.google.com/something"), Uri.parse("http://google.com")));

        // And facebook which has been moved from Disconnect into social
        assertFalse(matcher.matches(Uri.parse("http://facebook.fr"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://facebook.fr"), Uri.parse("http://facebook.com")));

        // Now disable everything - all sites should work:
        prefs.edit()
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_ads), false)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_analytics), false)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_other), false)
                .putBoolean(RuntimeEnvironment.application.getString(R.string.pref_key_privacy_block_social), false)
                .apply();

        // ads:
        assertFalse(matcher.matches(Uri.parse("http://admeld.com/foobar"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://admeld.com/foobar"), Uri.parse("http://google.com")));
        // analytics:
        assertFalse(matcher.matches(Uri.parse("http://google-analytics.com/foobar"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://google-analytics.com/foobar"), Uri.parse("http://google.com")));
        // social:
        assertFalse(matcher.matches(Uri.parse("http://plus.google.com/something"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://plus.google.com/something"), Uri.parse("http://google.com")));

        // Facebook is special in that we move it from "Disconnect" into social:
        assertFalse(matcher.matches(Uri.parse("http://facebook.fr"), Uri.parse("http://mozilla.org")));
        assertFalse(matcher.matches(Uri.parse("http://facebook.fr"), Uri.parse("http://facebook.com")));
    }
}
