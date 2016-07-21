/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.distribution;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

/**
 * Client for accessing data from Android's "partner browser customizations" content provider.
 */
public class PartnerBrowserCustomizationsClient {
    private static final Uri CONTENT_URI = Uri.parse("content://com.android.partnerbrowsercustomizations");

    private static final Uri HOMEPAGE_URI = CONTENT_URI.buildUpon().path("homepage").build();

    private static final String COLUMN_HOMEPAGE = "homepage";

    /**
     * Returns the partner homepage or null if it could not be read from the content provider.
     */
    public static String getHomepage(Context context) {
        Cursor cursor = context.getContentResolver().query(
                HOMEPAGE_URI, new String[] { COLUMN_HOMEPAGE }, null, null, null);

        if (cursor == null) {
            return null;
        }

        try {
            if (!cursor.moveToFirst()) {
                return null;
            }

            return cursor.getString(cursor.getColumnIndexOrThrow(COLUMN_HOMEPAGE));
        } finally {
            cursor.close();
        }
    }
}
