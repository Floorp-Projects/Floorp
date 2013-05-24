/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.entity.BufferedHttpEntity;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;
import android.net.http.AndroidHttpClient;
import android.os.Handler;
import android.support.v4.util.LruCache;
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
    public static final long FAILED_EXPIRY_NEVER = -1;

    private static int sFaviconSmallSize = -1;
    private static int sFaviconLargeSize = -1;

    private Context mContext;

    private Map<Long,LoadFaviconTask> mLoadTasks;
    private long mNextFaviconLoadId;
    private LruCache<String, Bitmap> mFaviconsCache;
    private LruCache<String, Long> mFailedCache;
    private LruCache<String, Integer> mColorCache;
    private static final String USER_AGENT = GeckoAppShell.getGeckoInterface().getDefaultUAString();
    private AndroidHttpClient mHttpClient;

    public interface OnFaviconLoadedListener {
        public void onFaviconLoaded(String url, Bitmap favicon);
    }

    public Favicons() {
        Log.d(LOGTAG, "Creating Favicons instance");

        mLoadTasks = Collections.synchronizedMap(new HashMap<Long,LoadFaviconTask>());
        mNextFaviconLoadId = 0;

        // Create a favicon memory cache that have up to 1mb of size
        mFaviconsCache = new LruCache<String, Bitmap>(1024 * 1024) {
            @Override
            protected int sizeOf(String url, Bitmap image) {
                return image.getRowBytes() * image.getHeight();
            }
        };

        // Create a failed favicon memory cache that has up to 64 entries
        mFailedCache = new LruCache<String, Long>(64);

        // Create a cache to store favicon dominant colors
        mColorCache = new LruCache<String, Integer>(256);
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
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (listener != null)
                    listener.onFaviconLoaded(pageUrl, image);
            }
        });
    }

    public String getFaviconUrlForPageUrl(String pageUrl) {
        return BrowserDB.getFaviconUrlForHistoryUrl(mContext.getContentResolver(), pageUrl);
    }

    public long loadFavicon(String pageUrl, String faviconUrl, boolean persist,
            OnFaviconLoadedListener listener) {

        // Handle the case where page url is empty
        if (pageUrl == null || pageUrl.length() == 0) {
            dispatchResult(null, null, listener);
            return -1;
        }

        // Check if favicon has failed
        if (isFailedFavicon(pageUrl)) {
            dispatchResult(pageUrl, null, listener);
            return -1;
        }

        // Check if favicon is mem cached
        Bitmap image = getFaviconFromMemCache(pageUrl);
        if (image != null) {
            dispatchResult(pageUrl, image, listener);
            return -1;
        }

        LoadFaviconTask task = new LoadFaviconTask(ThreadUtils.getBackgroundHandler(), pageUrl, faviconUrl, persist, listener);

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

    public boolean isFailedFavicon(String pageUrl) {
        Long fetchTime = mFailedCache.get(pageUrl);
        if (fetchTime == null)
            return false;
        // We don't have any other rules yet.
        return true;
    }

    public void putFaviconInFailedCache(String pageUrl, long fetchTime) {
        mFailedCache.put(pageUrl, fetchTime);
    }

    public void clearFailedCache() {
        mFailedCache.evictAll();
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

    public void close() {
        Log.d(LOGTAG, "Closing Favicons database");

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

    private static class FaviconsInstanceHolder {
        private static final Favicons INSTANCE = new Favicons();
    }

    public static Favicons getInstance() {
       return Favicons.FaviconsInstanceHolder.INSTANCE;
    }

    public boolean isLargeFavicon(Bitmap image) {
        return image.getWidth() > sFaviconSmallSize || image.getHeight() > sFaviconSmallSize;
    }

    public Bitmap scaleImage(Bitmap image) {
        // If the icon is larger than 16px, scale it to sFaviconLargeSize.
        // Otherwise, scale it to sFaviconSmallSize.
        if (isLargeFavicon(image)) {
            image = Bitmap.createScaledBitmap(image, sFaviconLargeSize, sFaviconLargeSize, false);
        } else {
            image = Bitmap.createScaledBitmap(image, sFaviconSmallSize, sFaviconSmallSize, false);
        }
        return image;
    }

    public int getFaviconColor(Bitmap image, String key) {
        Integer color = mColorCache.get(key);
        if (color != null) {
            return color;
        }

        color = BitmapUtils.getDominantColor(image);
        mColorCache.put(key, color);
        return color;
    }

    public void attachToContext(Context context) {
        mContext = context;
        if (sFaviconSmallSize < 0) {
            sFaviconSmallSize = Math.round(mContext.getResources().getDimension(R.dimen.awesomebar_row_favicon_size_small));
        }
        if (sFaviconLargeSize < 0) {
            sFaviconLargeSize = Math.round(mContext.getResources().getDimension(R.dimen.awesomebar_row_favicon_size_large));
        }
    }

    private class LoadFaviconTask extends UiAsyncTask<Void, Void, Bitmap> {
        private long mId;
        private String mPageUrl;
        private String mFaviconUrl;
        private OnFaviconLoadedListener mListener;
        private boolean mPersist;

        public LoadFaviconTask(Handler backgroundThreadHandler,
                               String pageUrl, String faviconUrl, boolean persist,
                               OnFaviconLoadedListener listener) {
            super(backgroundThreadHandler);

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
        private void saveFaviconToDb(final Bitmap favicon) {
            if (!mPersist) {
                return;
            }

            ContentResolver resolver = mContext.getContentResolver();
            BrowserDB.updateFaviconForUrl(resolver, mPageUrl, favicon, mFaviconUrl);
        }

        // Runs in background thread
        private Bitmap downloadFavicon(URL faviconUrl) {
            if (mFaviconUrl.startsWith("jar:jar:")) {
                return GeckoJarReader.getBitmap(mContext.getResources(), mFaviconUrl);
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
                HttpResponse response = getHttpClient().execute(request);
                if (response == null)
                    return null;
                if (response.getStatusLine() != null) {
                    // Was the response a failure?
                    int status = response.getStatusLine().getStatusCode();
                    if (status >= 400) {
                        putFaviconInFailedCache(mPageUrl, FAILED_EXPIRY_NEVER);
                        return null;
                    }
                }

                HttpEntity entity = response.getEntity();
                if (entity == null)
                    return null;
                if (entity.getContentType() != null) {
                    // Is the content type valid? Might be a captive portal.
                    String contentType = entity.getContentType().getValue();
                    if (contentType.indexOf("image") == -1)
                        return null;
                }

                BufferedHttpEntity bufferedEntity = new BufferedHttpEntity(entity);
                InputStream contentStream = bufferedEntity.getContent();
                image = BitmapUtils.decodeStream(contentStream);
                contentStream.close();
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

            String storedFaviconUrl = getFaviconUrlForPageUrl(mPageUrl);
            if (storedFaviconUrl != null && storedFaviconUrl.equals(mFaviconUrl)) {
                image = loadFaviconFromDb();
                if (image != null && image.getWidth() > 0 && image.getHeight() > 0)
                    return scaleImage(image);
            }

            if (isCancelled())
                return null;

            image = downloadFavicon(faviconUrl);

            if (image != null && image.getWidth() > 0 && image.getHeight() > 0) {
                saveFaviconToDb(image);
                image = scaleImage(image);
            } else {
                image = null;
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
