/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.http.AndroidHttpClient;
import android.os.AsyncTask;
import android.util.Log;

import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.db.BrowserDB;

import org.apache.http.HttpEntity;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.entity.BufferedHttpEntity;

public class Favicons {
    private static final String LOGTAG = "GeckoFavicons";

    public static final long NOT_LOADING = 0;

    private Context mContext;
    private DatabaseHelper mDbHelper;

    private Map<Long,LoadFaviconTask> mLoadTasks;
    private long mNextFaviconLoadId;
    private static final String USER_AGENT = GeckoApp.mAppContext.getDefaultUAString();
    private AndroidHttpClient mHttpClient;

    public interface OnFaviconLoadedListener {
        public void onFaviconLoaded(String url, Drawable favicon);
    }

    private class DatabaseHelper extends SQLiteOpenHelper {
        private static final String DATABASE_NAME = "favicon_urls.db";
        private static final String TABLE_NAME = "favicon_urls";
        private static final int DATABASE_VERSION = 1;

        private static final String COLUMN_ID = "_id";
        private static final String COLUMN_FAVICON_URL = "favicon_url";
        private static final String COLUMN_PAGE_URL = "page_url";

        DatabaseHelper(Context context) {
            super(context, DATABASE_NAME, null, DATABASE_VERSION);
            Log.d(LOGTAG, "Creating DatabaseHelper");
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            Log.d(LOGTAG, "Creating database for favicon URLs");

            db.execSQL("CREATE TABLE " + TABLE_NAME + " (" +
                       COLUMN_ID + " INTEGER PRIMARY KEY," +
                       COLUMN_FAVICON_URL + " TEXT NOT NULL," +
                       COLUMN_PAGE_URL + " TEXT UNIQUE NOT NULL" +
                       ");");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            Log.w(LOGTAG, "Upgrading favicon URLs database from version " +
                  oldVersion + " to " + newVersion + ", which will destroy all old data");

            // Drop table completely
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_NAME);

            // Recreate database
            onCreate(db);
        }

        public String getFaviconUrlForPageUrl(String pageUrl) {
            Log.d(LOGTAG, "Calling getFaviconUrlForPageUrl() for " + pageUrl);

            SQLiteDatabase db = mDbHelper.getReadableDatabase();

            SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
            qb.setTables(TABLE_NAME);

            Cursor c = qb.query(
                db,
                new String[] { COLUMN_FAVICON_URL },
                COLUMN_PAGE_URL + " = ?",
                new String[] { pageUrl },
                null, null, null
            );

            if (!c.moveToFirst()) {
                c.close();
                return null;
            }

            String url = c.getString(c.getColumnIndexOrThrow(COLUMN_FAVICON_URL));
            c.close();
            return url;
        }

        public void setFaviconUrlForPageUrl(String pageUrl, String faviconUrl) {
            Log.d(LOGTAG, "Calling setFaviconUrlForPageUrl() for " + pageUrl);

            SQLiteDatabase db = mDbHelper.getWritableDatabase();

            ContentValues values = new ContentValues();
            values.put(COLUMN_FAVICON_URL, faviconUrl);
            values.put(COLUMN_PAGE_URL, pageUrl);

            db.replace(TABLE_NAME, null, values);
        }


        public void clearFavicons() {
            SQLiteDatabase db = mDbHelper.getWritableDatabase();
            db.delete(TABLE_NAME, null, null);
        }
    }

    public Favicons(Context context) {
        Log.d(LOGTAG, "Creating Favicons instance");

        mContext = context;
        mDbHelper = new DatabaseHelper(context);

        mLoadTasks = Collections.synchronizedMap(new HashMap<Long,LoadFaviconTask>());
        mNextFaviconLoadId = 0;
    }

    private synchronized AndroidHttpClient getHttpClient() {
        if (mHttpClient != null)
            return mHttpClient;

        mHttpClient = AndroidHttpClient.newInstance(USER_AGENT);
        return mHttpClient;
    }

    public String getFaviconUrlForPageUrl(String pageUrl) {
        return mDbHelper.getFaviconUrlForPageUrl(pageUrl);
    }

    public long loadFavicon(String pageUrl, String faviconUrl,
            OnFaviconLoadedListener listener) {

        // Handle the case where page url is empty
        if (pageUrl == null || pageUrl.length() == 0) {
            if (listener != null)
                listener.onFaviconLoaded(null, null);
        }

        LoadFaviconTask task = new LoadFaviconTask(pageUrl, faviconUrl, listener);

        long taskId = task.getId();
        mLoadTasks.put(taskId, task);

        task.execute();

        Log.d(LOGTAG, "Calling loadFavicon() with URL = " + pageUrl +
                        " and favicon URL = " + faviconUrl +
                        " (" + taskId + ")");

        return taskId;
    }

    public boolean cancelFaviconLoad(long taskId) {
        Log.d(LOGTAG, "Requesting cancelation of favicon load (" + taskId + ")");

        boolean cancelled = false;
        synchronized (mLoadTasks) {
            if (!mLoadTasks.containsKey(taskId))
                return false;

            Log.d(LOGTAG, "Cancelling favicon load (" + taskId + ")");

            LoadFaviconTask task = mLoadTasks.get(taskId);
            cancelled = task.cancel(false);
        }
        return cancelled;
    }

    public void clearFavicons() {
        mDbHelper.clearFavicons();
    }

    public void close() {
        Log.d(LOGTAG, "Closing Favicons database");
        mDbHelper.close();

        // Cancel any pending tasks
        synchronized (mLoadTasks) {
            Set<Long> taskIds = mLoadTasks.keySet();
            Iterator<Long> iter = taskIds.iterator();
            while (iter.hasNext()) {
                long taskId = iter.next();
                cancelFaviconLoad(taskId);
            }
        }
        if (mHttpClient != null)
            mHttpClient.close();
    }

    private class LoadFaviconTask extends AsyncTask<Void, Void, BitmapDrawable> {
        private long mId;
        private String mPageUrl;
        private String mFaviconUrl;
        private OnFaviconLoadedListener mListener;

        public LoadFaviconTask(String pageUrl, String faviconUrl, OnFaviconLoadedListener listener) {
            synchronized(this) {
                mId = ++mNextFaviconLoadId;
            }

            mPageUrl = pageUrl;
            mFaviconUrl = faviconUrl;
            mListener = listener;

            Log.d(LOGTAG, "Creating LoadFaviconTask with URL = " + pageUrl +
                          " and favicon URL = " + faviconUrl);
        }

        // Runs in background thread
        private BitmapDrawable loadFaviconFromDb() {
            Log.d(LOGTAG, "Loading favicon from DB for URL = " + mPageUrl);

            ContentResolver resolver = mContext.getContentResolver();
            BitmapDrawable favicon = BrowserDB.getFaviconForUrl(resolver, mPageUrl);

            if (favicon != null)
                Log.d(LOGTAG, "Loaded favicon from DB successfully for URL = " + mPageUrl);

            return favicon;
        }

        // Runs in background thread
        private void saveFaviconToDb(BitmapDrawable favicon) {
            // since the Async task can run this on any number of threads in the
            // pool, we need to protect against inserting the same url twice
            synchronized(mDbHelper) {
                Log.d(LOGTAG, "Saving favicon on browser database for URL = " + mPageUrl);
                ContentResolver resolver = mContext.getContentResolver();
                BrowserDB.updateFaviconForUrl(resolver, mPageUrl, favicon);

                Log.d(LOGTAG, "Saving favicon URL for URL = " + mPageUrl);
                mDbHelper.setFaviconUrlForPageUrl(mPageUrl, mFaviconUrl);
            }
        }

        // Runs in background thread
        private BitmapDrawable downloadFavicon(URL faviconUrl) {
            Log.d(LOGTAG, "Downloading favicon for URL = " + mPageUrl +
                          " with favicon URL = " + mFaviconUrl);

            if (mFaviconUrl.startsWith("jar:jar:")) {
                return GeckoJarReader.getBitmapDrawable(mFaviconUrl);
            }

            URI uri;
            try {
                uri = faviconUrl.toURI();
            } catch (URISyntaxException e) {
                Log.d(LOGTAG, "Could not get URI for favicon URL: " + mFaviconUrl);
                return null;
            }

            // only get favicons for HTTP/HTTPS
            String scheme = uri.getScheme();
            if (!"http".equals(scheme) && !"https".equals(scheme))
                return null;

            // skia decoder sometimes returns null; workaround is to use BufferedHttpEntity
            // http://groups.google.com/group/android-developers/browse_thread/thread/171b8bf35dbbed96/c3ec5f45436ceec8?lnk=raot 
            BitmapDrawable image = null;
            try {
                HttpGet request = new HttpGet(faviconUrl.toURI());
                HttpEntity entity = getHttpClient().execute(request).getEntity();
                BufferedHttpEntity bufferedEntity = new BufferedHttpEntity(entity);
                InputStream contentStream = bufferedEntity.getContent();
                image = (BitmapDrawable) Drawable.createFromStream(contentStream, "src");
            } catch (Exception e) {
                Log.e(LOGTAG, "Error reading favicon", e);
            }

            return image;
        }

        @Override
        protected BitmapDrawable doInBackground(Void... unused) {
            BitmapDrawable image = null;

            if (isCancelled())
                return null;

            URL faviconUrl = null;

            // Handle the case of malformed favicon URL
            try {
                // If favicon is empty, fallback to default favicon URI
                if (mFaviconUrl == null || mFaviconUrl.length() == 0) {
                    // Handle the case of malformed URL
                    URL pageUrl = null;
                    pageUrl = new URL(mPageUrl);

                    faviconUrl = new URL(pageUrl.getProtocol(), pageUrl.getAuthority(), "/favicon.ico");
                    mFaviconUrl = faviconUrl.toString();
                } else {
                    faviconUrl = new URL(mFaviconUrl);
                }
            } catch (MalformedURLException e) {
                Log.d(LOGTAG, "The provided favicon URL is not valid", e);
                return null;
            }

            Log.d(LOGTAG, "Favicon URL is now: " + mFaviconUrl);

            if (isCancelled())
                return null;

            String storedFaviconUrl = mDbHelper.getFaviconUrlForPageUrl(mPageUrl);
            if (storedFaviconUrl != null && storedFaviconUrl.equals(mFaviconUrl)) {
                image = loadFaviconFromDb();
                if (image != null)
                    return image;
            }

            if (isCancelled())
                return null;

            image = downloadFavicon(faviconUrl);

            if (image != null) {
                Log.d(LOGTAG, "Downloaded favicon successfully for URL = " + mPageUrl);
                saveFaviconToDb(image);
            }

            return image;
        }

        @Override
        protected void onPostExecute(final BitmapDrawable image) {
            Log.d(LOGTAG, "LoadFaviconTask finished for URL = " + mPageUrl +
                          " (" + mId + ")");

            mLoadTasks.remove(mId);

            if (mListener != null) {
                // We want to always run the listener on UI thread
                GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                    public void run() {
                        mListener.onFaviconLoaded(mPageUrl, image);
                    }
                });
            }
        }

        @Override
        protected void onCancelled() {
            Log.d(LOGTAG, "LoadFaviconTask cancelled for URL = " + mPageUrl +
                          " (" + mId + ")");

            mLoadTasks.remove(mId);

            // Note that we don't call the listener callback if the
            // favicon load is cancelled.
        }

        public long getId() {
            return mId;
        }
    }
}
