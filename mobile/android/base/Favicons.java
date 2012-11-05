/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.LruCache;

import org.apache.http.HttpEntity;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.entity.BufferedHttpEntity;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.http.AndroidHttpClient;
import android.os.AsyncTask;
import android.util.Log;

import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

public class Favicons {
    private static final String LOGTAG = "GeckoFavicons";

    public static final long NOT_LOADING = 0;

    private Context mContext;
    private DatabaseHelper mDbHelper;

    private Map<Long,LoadFaviconTask> mLoadTasks;
    private long mNextFaviconLoadId;
    private LruCache<String, Bitmap> mFaviconsCache;
    private static final String USER_AGENT = GeckoApp.mAppContext.getDefaultUAString();
    private AndroidHttpClient mHttpClient;

    public interface OnFaviconLoadedListener {
        public void onFaviconLoaded(String url, Bitmap favicon);
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

        // Create a favicon memory cache that have up to 1mb of size
        mFaviconsCache = new LruCache<String, Bitmap>(1024 * 1024) {
            @Override
            protected int sizeOf(String url, Bitmap image) {
                return image.getRowBytes() * image.getHeight();
            }
        };
    }

    private synchronized AndroidHttpClient getHttpClient() {
        if (mHttpClient != null)
            return mHttpClient;

        mHttpClient = AndroidHttpClient.newInstance(USER_AGENT);
        return mHttpClient;
    }

    private void dispatchResult(final String pageUrl, final Bitmap image,
            final OnFaviconLoadedListener listener) {
        if (pageUrl != null && image != null)
            putFaviconInMemCache(pageUrl, image);

        // We want to always run the listener on UI thread
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                if (listener != null)
                    listener.onFaviconLoaded(pageUrl, image);
            }
        });
    }

    public String getFaviconUrlForPageUrl(String pageUrl) {
        return mDbHelper.getFaviconUrlForPageUrl(pageUrl);
    }

    public long loadFavicon(String pageUrl, String faviconUrl, boolean persist,
            OnFaviconLoadedListener listener) {

        // Handle the case where page url is empty
        if (pageUrl == null || pageUrl.length() == 0) {
            dispatchResult(null, null, listener);
            return -1;
        }

        // Check if favicon is mem cached
        Bitmap image = getFaviconFromMemCache(pageUrl);
        if (image != null) {
            dispatchResult(pageUrl, image, listener);
            return -1;
        }

        LoadFaviconTask task = new LoadFaviconTask(pageUrl, faviconUrl, persist, listener);

        long taskId = task.getId();
        mLoadTasks.put(taskId, task);

        task.execute();

        return taskId;
    }

    public Bitmap getFaviconFromMemCache(String pageUrl) {
        return mFaviconsCache.get(pageUrl);
    }

    public void putFaviconInMemCache(String pageUrl, Bitmap image) {
        mFaviconsCache.put(pageUrl, image);
    }

    public void clearMemCache() {
        mFaviconsCache.evictAll();
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

    private class LoadFaviconTask extends AsyncTask<Void, Void, Bitmap> {
        private long mId;
        private String mPageUrl;
        private String mFaviconUrl;
        private OnFaviconLoadedListener mListener;
        private boolean mPersist;

        public LoadFaviconTask(String pageUrl, String faviconUrl, boolean persist,
                OnFaviconLoadedListener listener) {
            synchronized(this) {
                mId = ++mNextFaviconLoadId;
            }

            mPageUrl = pageUrl;
            mFaviconUrl = faviconUrl;
            mListener = listener;
            mPersist = persist;
        }

        // Runs in background thread
        private Bitmap loadFaviconFromDb() {
            ContentResolver resolver = mContext.getContentResolver();
            return BrowserDB.getFaviconForUrl(resolver, mPageUrl);
        }

        // Runs in background thread
        private void saveFaviconToDb(Bitmap favicon) {
            if (!mPersist) {
                return;
            }

            // since the Async task can run this on any number of threads in the
            // pool, we need to protect against inserting the same url twice
            synchronized(mDbHelper) {
                ContentResolver resolver = mContext.getContentResolver();
                BrowserDB.updateFaviconForUrl(resolver, mPageUrl, favicon);

                mDbHelper.setFaviconUrlForPageUrl(mPageUrl, mFaviconUrl);
            }
        }

        // Runs in background thread
        private Bitmap downloadFavicon(URL faviconUrl) {
            if (mFaviconUrl.startsWith("jar:jar:")) {
                BitmapDrawable d = GeckoJarReader.getBitmapDrawable(mContext.getResources(), mFaviconUrl);
                return d.getBitmap();
            }

            URI uri;
            try {
                uri = faviconUrl.toURI();
            } catch (URISyntaxException e) {
                Log.d(LOGTAG, "Could not get URI for favicon");
                return null;
            }

            // only get favicons for HTTP/HTTPS
            String scheme = uri.getScheme();
            if (!"http".equals(scheme) && !"https".equals(scheme))
                return null;

            // skia decoder sometimes returns null; workaround is to use BufferedHttpEntity
            // http://groups.google.com/group/android-developers/browse_thread/thread/171b8bf35dbbed96/c3ec5f45436ceec8?lnk=raot 
            Bitmap image = null;
            try {
                HttpGet request = new HttpGet(faviconUrl.toURI());
                HttpEntity entity = getHttpClient().execute(request).getEntity();
                BufferedHttpEntity bufferedEntity = new BufferedHttpEntity(entity);
                InputStream contentStream = bufferedEntity.getContent();
                image = BitmapFactory.decodeStream(contentStream);
            } catch (Exception e) {
                Log.e(LOGTAG, "Error reading favicon", e);
            }

            return image;
        }

        @Override
        protected Bitmap doInBackground(Void... unused) {
            Bitmap image = null;

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
                Log.d(LOGTAG, "The provided favicon URL is not valid");
                return null;
            }

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
                saveFaviconToDb(image);
            }

            return image;
        }

        @Override
        protected void onPostExecute(final Bitmap image) {
            mLoadTasks.remove(mId);
            dispatchResult(mPageUrl, image, mListener);
        }

        @Override
        protected void onCancelled() {
            mLoadTasks.remove(mId);

            // Note that we don't call the listener callback if the
            // favicon load is cancelled.
        }

        public long getId() {
            return mId;
        }
    }
}
