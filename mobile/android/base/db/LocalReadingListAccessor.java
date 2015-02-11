/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

public class LocalReadingListAccessor implements ReadingListAccessor {
    private static final String LOG_TAG = "GeckoReadingListAcc";

    private final Uri mReadingListUriWithProfile;

    public LocalReadingListAccessor(final String profile) {
        mReadingListUriWithProfile = DBUtils.appendProfile(profile, BrowserContract.ReadingListItems.CONTENT_URI);
    }

    @Override
    public int getCount(ContentResolver cr) {
        final String[] columns = new String[]{BrowserContract.ReadingListItems._ID};
        final Cursor cursor = cr.query(mReadingListUriWithProfile, columns, null, null, null);
        int count = 0;

        try {
            count = cursor.getCount();
        } finally {
            cursor.close();
        }

        Log.d(LOG_TAG, "Got count " + count + " for reading list.");
        return count;
    }

    @Override
    public Cursor getReadingList(ContentResolver cr) {
        return cr.query(mReadingListUriWithProfile,
                        BrowserContract.ReadingListItems.DEFAULT_PROJECTION,
                        null,
                        null,
                        null);
    }

    @Override
    public Cursor getReadingListUnfetched(ContentResolver cr) {
        return cr.query(mReadingListUriWithProfile,
                        new String[] { BrowserContract.ReadingListItems._ID, BrowserContract.ReadingListItems.URL },
                        BrowserContract.ReadingListItems.CONTENT_STATUS + " = " + BrowserContract.ReadingListItems.STATUS_UNFETCHED,
                        null,
                        null);
    }

    @Override
    public boolean isReadingListItem(ContentResolver cr, String uri) {
        final Cursor c = cr.query(mReadingListUriWithProfile,
                                  new String[] { BrowserContract.ReadingListItems._ID },
                                  BrowserContract.ReadingListItems.URL + " = ? ",
                                  new String[] { uri },
                                  null);

        if (c == null) {
            Log.e(LOG_TAG, "Null cursor in isReadingListItem");
            return false;
        }

        try {
            return c.getCount() > 0;
        } finally {
            c.close();
        }
    }


    @Override
    public void addReadingListItem(ContentResolver cr, ContentValues values) {
        // Check that required fields are present.
        for (String field: BrowserContract.ReadingListItems.REQUIRED_FIELDS) {
            if (!values.containsKey(field)) {
                throw new IllegalArgumentException("Missing required field for reading list item: " + field);
            }
        }

        // Clear delete flag if necessary
        values.put(BrowserContract.ReadingListItems.IS_DELETED, 0);

        // Restore deleted record if possible
        final Uri insertUri = mReadingListUriWithProfile
                .buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true")
                .build();

        final int updated = cr.update(insertUri,
                values,
                BrowserContract.ReadingListItems.URL + " = ? ",
                new String[] { values.getAsString(BrowserContract.ReadingListItems.URL) });

        Log.d(LOG_TAG, "Updated " + updated + " rows to new modified time.");
    }

    @Override
    public void updateReadingListItem(ContentResolver cr, ContentValues values) {
        if (!values.containsKey(BrowserContract.ReadingListItems._ID)) {
            throw new IllegalArgumentException("Cannot update reading list item without an ID");
        }

        final int updated = cr.update(mReadingListUriWithProfile,
                                      values,
                                      BrowserContract.ReadingListItems._ID + " = ? ",
                                      new String[] { values.getAsString(BrowserContract.ReadingListItems._ID) });

        Log.d(LOG_TAG, "Updated " + updated + " reading list rows.");
    }

    @Override
    public void removeReadingListItemWithURL(ContentResolver cr, String uri) {
        cr.delete(mReadingListUriWithProfile, BrowserContract.ReadingListItems.URL + " = ? ", new String[]{uri});
    }

    @Override
    public void registerContentObserver(Context context, ContentObserver observer) {
        context.getContentResolver().registerContentObserver(mReadingListUriWithProfile, false, observer);
    }
}
