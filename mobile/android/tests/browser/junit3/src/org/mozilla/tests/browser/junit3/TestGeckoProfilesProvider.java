/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.tests.browser.junit3;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;
import android.test.InstrumentationTestCase;
import org.mozilla.gecko.db.BrowserContract;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

public class TestGeckoProfilesProvider extends InstrumentationTestCase {
    private static final String[] NAME_AND_PATH = new String[] { BrowserContract.Profiles.NAME, BrowserContract.Profiles.PATH };

    /**
     * Ensure that the default profile is found in the results from the provider.
     */
    public void testQueryDefault() throws RemoteException {
        final ContentResolver contentResolver = getInstrumentation().getContext().getContentResolver();
        final Uri uri = BrowserContract.PROFILES_AUTHORITY_URI.buildUpon().appendPath("profiles").build();
        final Cursor c = contentResolver.query(uri, NAME_AND_PATH, null, null, null);
        assertNotNull(c);
        try {
            assertTrue(c.moveToFirst());
            assertTrue(c.getCount() > 0);
            Map<String, String> profiles = new HashMap<String, String>();
            while (!c.isAfterLast()) {
                final String name = c.getString(0);
                final String path = c.getString(1);
                profiles.put(name, path);
                c.moveToNext();
            }

            assertTrue(profiles.containsKey("default"));
            final String path = profiles.get("default");
            assertTrue(path.endsWith(".default"));          // It's the right profile...
            assertTrue(path.startsWith("/data/"));          // ... in the 'data' dir...
            assertTrue(path.contains("/mozilla/"));         // ... in the 'mozilla' dir.
            assertTrue(new File(path).exists());
        } finally {
            c.close();
        }
    }
}
