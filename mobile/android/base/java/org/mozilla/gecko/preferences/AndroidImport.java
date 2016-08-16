/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.ContentValues;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.IconsHelper;
import org.mozilla.gecko.icons.storage.DiskStorage;

import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.Context;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;
import android.provider.BaseColumns;
import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;

public class AndroidImport implements Runnable {
    /**
     * The Android M SDK removed several fields and methods from android.provider.Browser. This class is used as a
     * replacement to support building with the new SDK but at the same time still use these fields on lower Android
     * versions.
     */
    private static class LegacyBrowserProvider {
        public static final Uri BOOKMARKS_URI = Uri.parse("content://browser/bookmarks");

        // Incomplete: This are just the fields we currently use in our code base
        public static class BookmarkColumns implements BaseColumns {
            public static final String URL = "url";
            public static final String VISITS = "visits";
            public static final String DATE = "date";
            public static final String BOOKMARK = "bookmark";
            public static final String TITLE = "title";
            public static final String CREATED = "created";
            public static final String FAVICON = "favicon";
        }
    }

    public static final Uri SAMSUNG_BOOKMARKS_URI = Uri.parse("content://com.sec.android.app.sbrowser.browser/bookmarks");
    public static final Uri SAMSUNG_HISTORY_URI = Uri.parse("content://com.sec.android.app.sbrowser.browser/history");
    public static final String SAMSUNG_MANUFACTURER = "samsung";

    private static final String LOGTAG = "AndroidImport";
    private final Context mContext;
    private final Runnable mOnDoneRunnable;
    private final ArrayList<ContentProviderOperation> mOperations;
    private final ContentResolver mCr;
    private final LocalBrowserDB mDB;
    private final boolean mImportBookmarks;
    private final boolean mImportHistory;

    public AndroidImport(Context context, Runnable onDoneRunnable,
                         boolean doBookmarks, boolean doHistory) {
        mContext = context;
        mOnDoneRunnable = onDoneRunnable;
        mOperations = new ArrayList<ContentProviderOperation>();
        mCr = mContext.getContentResolver();
        mDB = new LocalBrowserDB(GeckoProfile.get(context).getName());
        mImportBookmarks = doBookmarks;
        mImportHistory = doHistory;
    }

    public void mergeBookmarks() {
        Cursor cursor = null;
        try {
            cursor = query(LegacyBrowserProvider.BOOKMARKS_URI,
                               SAMSUNG_BOOKMARKS_URI,
                               LegacyBrowserProvider.BookmarkColumns.BOOKMARK + " = 1");

            if (cursor != null) {
                final int faviconCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.FAVICON);
                final int titleCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.TITLE);
                final int urlCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.URL);
                // http://code.google.com/p/android/issues/detail?id=17969
                final int createCol = cursor.getColumnIndex(LegacyBrowserProvider.BookmarkColumns.CREATED);

                cursor.moveToFirst();
                while (!cursor.isAfterLast()) {
                    String url = cursor.getString(urlCol);
                    String title = cursor.getString(titleCol);
                    long created;
                    if (createCol >= 0) {
                        created = cursor.getLong(createCol);
                    } else {
                        created = System.currentTimeMillis();
                    }
                    // Need to set it to the current time so Sync picks it up.
                    long modified = System.currentTimeMillis();
                    byte[] data = cursor.getBlob(faviconCol);
                    mDB.updateBookmarkInBatch(mCr, mOperations,
                                              url, title, null, -1,
                                              created, modified,
                                              BrowserContract.Bookmarks.DEFAULT_POSITION,
                                              null, Bookmarks.TYPE_BOOKMARK);
                    if (data != null) {
                        storeBitmap(data, url);
                    }
                    cursor.moveToNext();
                }
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }

        flushBatchOperations();
    }

    public void mergeHistory() {
        ArrayList<ContentValues> visitsToSynthesize = new ArrayList<>();
        Cursor cursor = null;
        try {
            cursor = query (LegacyBrowserProvider.BOOKMARKS_URI,
                                SAMSUNG_HISTORY_URI,
                                LegacyBrowserProvider.BookmarkColumns.BOOKMARK + " = 0 AND " +
                                LegacyBrowserProvider.BookmarkColumns.VISITS + " > 0");

            if (cursor != null) {
                final int dateCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.DATE);
                final int faviconCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.FAVICON);
                final int titleCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.TITLE);
                final int urlCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.URL);
                final int visitsCol = cursor.getColumnIndexOrThrow(LegacyBrowserProvider.BookmarkColumns.VISITS);

                cursor.moveToFirst();
                while (!cursor.isAfterLast()) {
                    String url = cursor.getString(urlCol);
                    String title = cursor.getString(titleCol);
                    long date = cursor.getLong(dateCol);
                    int visits = cursor.getInt(visitsCol);
                    byte[] data = cursor.getBlob(faviconCol);
                    mDB.updateHistoryInBatch(mCr, mOperations, url, title, date, visits);
                    if (data != null) {
                        storeBitmap(data, url);
                    }
                    ContentValues visitData = new ContentValues();
                    visitData.put(LocalBrowserDB.HISTORY_VISITS_DATE, date);
                    visitData.put(LocalBrowserDB.HISTORY_VISITS_URL, url);
                    visitData.put(LocalBrowserDB.HISTORY_VISITS_COUNT, visits);
                    visitsToSynthesize.add(visitData);
                    cursor.moveToNext();
                }
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }

        flushBatchOperations();

        // Now that we have flushed history records, we need to synthesize individual visits. We have
        // gathered information about all of the visits we need to synthesize into visitsForSynthesis.
        mDB.insertVisitsFromImportHistoryInBatch(mCr, mOperations, visitsToSynthesize);

        flushBatchOperations();
    }

    private void storeBitmap(byte[] data, String url) {
        if (TextUtils.isEmpty(url) || data == null) {
            return;
        }

        final Bitmap bitmap = BitmapFactory.decodeByteArray(data, 0, data.length);
        if (bitmap == null) {
            return;
        }

        final String iconUrl = IconsHelper.guessDefaultFaviconURL(url);
        if (iconUrl == null) {
            return;
        }

        final DiskStorage storage = DiskStorage.get(mContext);

        storage.putIcon(url, bitmap);
        storage.putMapping(url, iconUrl);
    }

    protected Cursor query(Uri mainUri, Uri fallbackUri, String condition) {
        final Cursor cursor = mCr.query(mainUri, null, condition, null, null);
        if (Build.MANUFACTURER.equals(SAMSUNG_MANUFACTURER) && (cursor == null || cursor.getCount() == 0)) {
            if (cursor != null) {
                cursor.close();
            }
            return mCr.query(fallbackUri, null, null, null, null);
        }
        return cursor;
    }

    protected void flushBatchOperations() {
        Log.d(LOGTAG, "Flushing " + mOperations.size() + " DB operations");
        try {
            // We don't really care for the results, this is best-effort.
            mCr.applyBatch(BrowserContract.AUTHORITY, mOperations);
        } catch (RemoteException e) {
            Log.e(LOGTAG, "Remote exception while updating db: ", e);
        } catch (OperationApplicationException e) {
            // Bug 716729 means this happens even in normal circumstances
            Log.d(LOGTAG, "Error while applying database updates: ", e);
        }
        mOperations.clear();
    }

    @Override
    public void run() {
        if (mImportBookmarks) {
            mergeBookmarks();
        }
        if (mImportHistory) {
            mergeHistory();
        }

        mOnDoneRunnable.run();
    }
}
