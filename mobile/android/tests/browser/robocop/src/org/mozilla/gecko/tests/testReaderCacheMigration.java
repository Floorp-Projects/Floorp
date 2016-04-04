/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserDatabaseHelper;

import java.io.File;
import java.io.IOException;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

/**
 * Tests that our readercache-migration works correctly.
 *
 * Our main concern is ensuring that the hashed path for a given url is the same in Java
 * as it was in JS, or else our (Java-based) migration will lose track of valid cached items.
 */
public class testReaderCacheMigration extends JavascriptBridgeTest {

    private final String[] TEST_DOMAINS = new String[] {
            "",
            "http://mozilla.org",
            "https://bugzilla.mozilla.org/show_bug.cgi?id=1234315#c41",
            "http://www.llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch.com/"
    };

    private static final String TEST_JS = "testReaderCacheMigration.js";

    /**
     * We compute the path-name in Java, and pass this through to JS, which conducts the actual
     * equality check. Our JavascriptBridge doesn't seem to support return values, so we need
     * to instead pass the computed path-name in at least one direction.
     */
    private void checkPathMatches(final String pageURL, final File cacheDir) {
        final String hashedName = BrowserDatabaseHelper.getReaderCacheFileNameForURL(pageURL);

        final File cacheFile = new File(cacheDir, hashedName);

        try {
            // We have to use the canonical path to match what the JS side will use. We could
            // instead just match on the file name, and not the path, but this helps
            // ensure that we've not broken any of the path finding either.
            getJS().syncCall("check_hashed_path_matches", pageURL, cacheFile.getCanonicalPath());
        } catch (IOException e) {
            fAssertTrue("Unable to getCanonicalPath(), this should never happen", false);
        }

    }

    public void testReaderCacheMigration() {
        blockForReadyAndLoadJS(TEST_JS);

        final File cacheDir = new File(GeckoProfile.get(getActivity()).getDir(), "readercache");

        for (final String URL : TEST_DOMAINS) {
            checkPathMatches(URL, cacheDir);
        }
    }
}
