/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons;


import android.content.ContentResolver;
import android.graphics.Bitmap;
import android.net.http.AndroidHttpClient;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Log;
import org.apache.http.Header;
import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.entity.BufferedHttpEntity;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;
import static org.mozilla.gecko.favicons.Favicons.sContext;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Class representing the asynchronous task to load a Favicon which is not currently in the in-memory
 * cache.
 * The implementation initially tries to get the Favicon from the database. Upon failure, the icon
 * is loaded from the internet.
 */
public class LoadFaviconTask extends UiAsyncTask<Void, Void, Bitmap> {
    private static final String LOGTAG = "LoadFaviconTask";

    // Access to this map needs to be synchronized prevent multiple jobs loading the same favicon
    // from executing concurrently.
    private static final HashMap<String, LoadFaviconTask> loadsInFlight = new HashMap<String, LoadFaviconTask>();

    public static final int FLAG_PERSIST = 1;
    public static final int FLAG_SCALE = 2;
    private static final int MAX_REDIRECTS_TO_FOLLOW = 5;

    private static AtomicInteger mNextFaviconLoadId = new AtomicInteger(0);
    private int mId;
    private String mPageUrl;
    private String mFaviconUrl;
    private OnFaviconLoadedListener mListener;
    private int mFlags;

    private final boolean mOnlyFromLocal;

    // Assuming square favicons, judging by width only is acceptable.
    protected int mTargetWidth;
    private final AtomicReference<LoadFaviconTask> mChainee = new AtomicReference<LoadFaviconTask>(null);
    private boolean mIsChaining;

    static AndroidHttpClient sHttpClient = AndroidHttpClient.newInstance(GeckoAppShell.getGeckoInterface().getDefaultUAString());

    public LoadFaviconTask(Handler backgroundThreadHandler,
                           String pageUrl, String faviconUrl, int flags,
                           OnFaviconLoadedListener listener) {
        this(backgroundThreadHandler, pageUrl, faviconUrl, flags, listener, -1, false);
    }
    public LoadFaviconTask(Handler backgroundThreadHandler,
                           String pageUrl, String faviconUrl, int flags,
                           OnFaviconLoadedListener aListener, int targetSize, boolean fromLocal) {
        super(backgroundThreadHandler);

        mId = mNextFaviconLoadId.incrementAndGet();

        mPageUrl = pageUrl;
        mFaviconUrl = faviconUrl;
        mListener = aListener;
        mFlags = flags;
        mTargetWidth = targetSize;
        mOnlyFromLocal = fromLocal;
    }

    // Runs in background thread
    private Bitmap loadFaviconFromDb() {
        ContentResolver resolver = sContext.getContentResolver();
        return BrowserDB.getFaviconForFaviconUrl(resolver, mFaviconUrl);
    }

    // Runs in background thread
    private void saveFaviconToDb(final Bitmap favicon) {
        if ((mFlags & FLAG_PERSIST) == 0) {
            return;
        }

        ContentResolver resolver = sContext.getContentResolver();
        BrowserDB.updateFaviconForUrl(resolver, mPageUrl, favicon, mFaviconUrl);
    }

    /**
     * Helper method for trying the download request to grab a Favicon.
     * @param faviconURI URL of Favicon to try and download
     * @return The HttpResponse containing the downloaded Favicon if successful, null otherwise.
     */
    private HttpResponse tryDownload(URI faviconURI) throws URISyntaxException, IOException {
        HashSet<String> visitedLinkSet = new HashSet<String>();
        visitedLinkSet.add(faviconURI.toString());
        return tryDownloadRecurse(faviconURI, visitedLinkSet);
    }
    private HttpResponse tryDownloadRecurse(URI faviconURI, HashSet<String> visited) throws URISyntaxException, IOException {
        if (visited.size() == MAX_REDIRECTS_TO_FOLLOW) {
            return null;
        }

        HttpGet request = new HttpGet(faviconURI);
        HttpResponse response = sHttpClient.execute(request);
        if (response == null) {
            return null;
        }

        if (response.getStatusLine() != null) {

            // Was the response a failure?
            int status = response.getStatusLine().getStatusCode();

            // Handle HTTP status codes requesting a redirect.
            if (status >= 300 && status < 400) {
                Header header = response.getFirstHeader("Location");

                // Handle mad webservers.
                if (header == null) {
                    return null;
                }

                String newURI = header.getValue();
                if (newURI == null || newURI.equals(faviconURI.toString())) {
                    return null;
                }

                if (visited.contains(newURI)) {
                    // Already been redirected here - abort.
                    return null;
                }

                visited.add(newURI);
                return tryDownloadRecurse(new URI(newURI), visited);
            }

            if (status >= 400) {
                return null;
            }
        }
        return response;
    }

    // Runs in background thread
    private Bitmap downloadFavicon(URI targetFaviconURI) {
        if (mFaviconUrl.startsWith("jar:jar:")) {
            return GeckoJarReader.getBitmap(sContext.getResources(), mFaviconUrl);
        }

        // only get favicons for HTTP/HTTPS
        String scheme = targetFaviconURI.getScheme();
        if (!"http".equals(scheme) && !"https".equals(scheme)) {
            return null;
        }

        // skia decoder sometimes returns null; workaround is to use BufferedHttpEntity
        // http://groups.google.com/group/android-developers/browse_thread/thread/171b8bf35dbbed96/c3ec5f45436ceec8?lnk=raot
        Bitmap image = null;
        try {
            // Try the URL we were given.
            HttpResponse response = tryDownload(targetFaviconURI);
            if (response == null) {
                return null;
            }

            HttpEntity entity = response.getEntity();
            if (entity == null) {
                return null;
            }

            BufferedHttpEntity bufferedEntity = new BufferedHttpEntity(entity);
            InputStream contentStream = null;
            try {
                contentStream = bufferedEntity.getContent();
                image = BitmapUtils.decodeStream(contentStream);
                contentStream.close();
            } finally {
                if (contentStream != null) {
                    contentStream.close();
                }
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Error reading favicon", e);
        }

        return image;
    }

    @Override
    protected Bitmap doInBackground(Void... unused) {
        if (isCancelled()) {
            return null;
        }

        String storedFaviconUrl;
        boolean isUsingDefaultURL = false;

        // Handle the case of malformed favicon URL.
        // If favicon is empty, fall back to the stored one.
        if (TextUtils.isEmpty(mFaviconUrl)) {
            // Try to get the favicon URL from the memory cache.
            storedFaviconUrl = Favicons.getFaviconURLForPageURLFromCache(mPageUrl);

            // If that failed, try to get the URL from the database.
            if (storedFaviconUrl == null) {
                storedFaviconUrl = Favicons.getFaviconUrlForPageUrl(mPageUrl);
                if (storedFaviconUrl != null) {
                    // If that succeeded, cache the URL loaded from the database in memory.
                    Favicons.putFaviconURLForPageURLInCache(mPageUrl, storedFaviconUrl);
                }
            }

            // If we found a faviconURL - use it.
            if (storedFaviconUrl != null) {
                mFaviconUrl = storedFaviconUrl;
            } else {
                // If we don't have a stored one, fall back to the default.
                mFaviconUrl = Favicons.guessDefaultFaviconURL(mPageUrl);
                isUsingDefaultURL = true;
            }
        }

        // Check if favicon has failed - if so, give up. We need this check because, sometimes, we
        // didn't know the real Favicon URL until we asked the database.
        if (Favicons.isFailedFavicon(mFaviconUrl)) {
            return null;
        }

        if (isCancelled()) {
            return null;
        }

        Bitmap image;
        // Determine if there is already an ongoing task to fetch the Favicon we desire.
        // If there is, just join the queue and wait for it to finish. If not, we carry on.
        synchronized(loadsInFlight) {
            // Another load of the current Favicon is already underway
            LoadFaviconTask existingTask = loadsInFlight.get(mFaviconUrl);
            if (existingTask != null && !existingTask.isCancelled()) {
                existingTask.chainTasks(this);
                mIsChaining = true;
            }

            // Replace the element in the map with the end of the chain, so a subsequent redundant
            // request created during the lifetime of this one will also get the result sent down -
            // without the need to chaining calls to chainTasks.
            loadsInFlight.put(mFaviconUrl, this);
        }

        if (mIsChaining) {
            // If we're chaining, abort.
            return null;
        }

        if (isCancelled()) {
            return null;
        }

        image = loadFaviconFromDb();
        if (image != null && image.getWidth() > 0 && image.getHeight() > 0) {
            return image;
        }

        if (mOnlyFromLocal || isCancelled()) {
            return null;
        }

        try {
            image = downloadFavicon(new URI(mFaviconUrl));
        } catch (URISyntaxException e) {
            Log.e(LOGTAG, "The provided favicon URL is not valid");
            return null;
        }

        // If we're not already trying the default URL, try it now.
        if (image == null && !isUsingDefaultURL) {
            try {
                image = downloadFavicon(new URI(Favicons.guessDefaultFaviconURL(mPageUrl)));
            } catch (URISyntaxException e){
                // Not interesting. It was an educated guess, anyway.
            }
        }

        if (image != null && image.getWidth() > 0 && image.getHeight() > 0) {
            saveFaviconToDb(image);
        } else {
            Favicons.putFaviconInFailedCache(mFaviconUrl);
        }

        return image;
    }

    @Override
    protected void onPostExecute(Bitmap image) {
        if (mIsChaining) {
            return;
        }

        // Put what we got in the memcache.
        Favicons.putFaviconInMemCache(mFaviconUrl, image);

        // Process the result - scale for the listener, chain if required.
        processResult(image);
    }

    private void processResult(Bitmap image) {
        Favicons.removeLoadTask(mId);

        Bitmap scaled = image;

        // Notify listeners, scaling if required.
        if (mTargetWidth != -1 && image != null &&  image.getWidth() != mTargetWidth) {
            scaled = Favicons.getSizedFaviconFromCache(mFaviconUrl, mTargetWidth);
        }

        Favicons.dispatchResult(mPageUrl, mFaviconUrl, scaled, mListener);

        // Take a snapshot of the chainee reference.
        final LoadFaviconTask chainee = mChainee.get();

        if (chainee != null) {
            // Propagate the result along the chain.
            // Note that we don't pass the scaled image -- the chainee might not want
            // the same size that this task's listener does.
            chainee.processResult(image);
            mChainee.set(null);
            return;
        }

        // If we find we had no chainee set, enter the monitor (Which is required to update the
        // mChainee reference) and check again. If we're still lacking a chainee, remove the
        // reference from loadsInFlight. Deals with chain growth racing with this call.
        synchronized(loadsInFlight) {
            if (mChainee.get() == null) {
                loadsInFlight.remove(mFaviconUrl);
                return;
            }
        }

        // Another element was added to the chain while we weren't looking...
        mChainee.get().processResult(image);
    }

    @Override
    protected void onCancelled() {
        Favicons.removeLoadTask(mId);

        synchronized(loadsInFlight) {
            if (mChainee.get() == null) {
                loadsInFlight.remove(mFaviconUrl);
            }
        }

        // Note that we don't call the listener callback if the
        // favicon load is cancelled.
    }

    /**
     * When the result of this job is ready, also notify the chainee of the result.
     * Used for aggregating concurrent requests for the same Favicon into a single actual request.
     * (Don't want to download a hundred instances of Google's Favicon at once, for example).
     *
     * @param aChainee LoadFaviconTask
     */
    private void chainTasks(LoadFaviconTask aChainee) {
        // Atomically update mChainee if it's null.
        if (mChainee.compareAndSet(null, aChainee)) {
            return;
        }

        // chainResults is only called within a synchronized block on loadsInFlight - so the value
        // can't have changed since the CAS above.
        mChainee.get().chainTasks(aChainee);
    }

    int getId() {
        return mId;
    }

    static void closeHTTPClient() {
        // This work must be done on a background thread because it shuts down
        // the connection pool, which typically involves closing a connection --
        // which counts as network activity.
        if (ThreadUtils.isOnBackgroundThread()) {
            if (sHttpClient != null) {
                sHttpClient.close();
            }
            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                LoadFaviconTask.closeHTTPClient();
            }
        });
    }
}
