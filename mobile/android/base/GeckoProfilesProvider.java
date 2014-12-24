/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.util.Map;
import java.util.Map.Entry;

import org.mozilla.gecko.GeckoProfileDirectories.NoMozillaDirectoryException;
import org.mozilla.gecko.db.BrowserContract;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.util.Log;

/**
 * This is not a per-profile provider. This provider allows read-only,
 * restricted access to certain attributes of Fennec profiles.
 */
public class GeckoProfilesProvider extends ContentProvider {
    private static final String LOG_TAG = "GeckoProfilesProvider";

    private static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    private static final int PROFILES = 100;
    private static final int PROFILES_NAME = 101;
    private static final int PROFILES_DEFAULT = 200;

    private static final String[] DEFAULT_ARGS = {
        BrowserContract.Profiles.NAME,
        BrowserContract.Profiles.PATH,
    };

    static {
        URI_MATCHER.addURI(BrowserContract.PROFILES_AUTHORITY, "profiles", PROFILES);
        URI_MATCHER.addURI(BrowserContract.PROFILES_AUTHORITY, "profiles/*", PROFILES_NAME);
        URI_MATCHER.addURI(BrowserContract.PROFILES_AUTHORITY, "default", PROFILES_DEFAULT);
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public boolean onCreate() {
        // Successfully loaded.
        return true;
    }

    private String[] profileValues(final String name, final String path, int len, int nameIndex, int pathIndex) {
        final String[] values = new String[len];
        if (nameIndex >= 0) {
            values[nameIndex] = name;
        }
        if (pathIndex >= 0) {
            values[pathIndex] = path;
        }
        return values;
    }

    protected void addRowForProfile(final MatrixCursor cursor, final int len, final int nameIndex, final int pathIndex, final String name, final String path) {
        if (path == null || name == null) {
            return;
        }

        cursor.addRow(profileValues(name, path, len, nameIndex, pathIndex));
    }

    protected Cursor getCursorForProfiles(final String[] args, Map<String, String> profiles) {
        // Compute the projection.
        int nameIndex = -1;
        int pathIndex = -1;
        for (int i = 0; i < args.length; ++i) {
            if (BrowserContract.Profiles.NAME.equals(args[i])) {
                nameIndex = i;
            } else if (BrowserContract.Profiles.PATH.equals(args[i])) {
                pathIndex = i;
            }
        }

        final MatrixCursor cursor = new MatrixCursor(args);
        for (Entry<String, String> entry : profiles.entrySet()) {
            addRowForProfile(cursor, args.length, nameIndex, pathIndex, entry.getKey(), entry.getValue());
        }
        return cursor;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {

        final String[] args = (projection == null) ? DEFAULT_ARGS : projection;

        final File mozillaDir;
        try {
            mozillaDir = GeckoProfileDirectories.getMozillaDirectory(getContext());
        } catch (NoMozillaDirectoryException e) {
            Log.d(LOG_TAG, "No Mozilla directory; cannot query for profiles. Assuming there are none.");
            return new MatrixCursor(projection);
        }

        final Map<String, String> matchingProfiles;

        final int match = URI_MATCHER.match(uri);
        switch (match) {
        case PROFILES:
            // Return all profiles.
            matchingProfiles = GeckoProfileDirectories.getAllProfiles(mozillaDir);
            break;
        case PROFILES_NAME:
            // Return data about the specified profile.
            final String name = uri.getLastPathSegment();
            matchingProfiles = GeckoProfileDirectories.getProfilesNamed(mozillaDir,
                                                                       name);
            break;
        case PROFILES_DEFAULT:
            matchingProfiles = GeckoProfileDirectories.getDefaultProfile(mozillaDir);
            break;
        default:
            throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        return getCursorForProfiles(args, matchingProfiles);
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new IllegalStateException("Inserts not supported.");
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new IllegalStateException("Deletes not supported.");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
                      String[] selectionArgs) {
        throw new IllegalStateException("Updates not supported.");
    }

}
