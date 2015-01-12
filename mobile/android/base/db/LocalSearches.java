/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;

/**
 * Helper class for dealing with the search provider inside Fennec.
 */
public class LocalSearches implements Searches {
    private final Uri uriWithProfile;

    public LocalSearches(String mProfile) {
        uriWithProfile = DBUtils.appendProfileWithDefault(mProfile, BrowserContract.SearchHistory.CONTENT_URI);
    }

    @Override
    public void insert(ContentResolver cr, String query) {
        final ContentValues values = new ContentValues();
        values.put(BrowserContract.SearchHistory.QUERY, query);
        cr.insert(uriWithProfile, values);
    }
}
