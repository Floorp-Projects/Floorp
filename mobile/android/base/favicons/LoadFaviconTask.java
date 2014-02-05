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
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.favicons.decoders.FaviconDecoder;
import org.mozilla.gecko.favicons.decoders.LoadFaviconResult;
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
import java.util.LinkedList;
import java.util.concurrent.atomic.AtomicInteger;

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
    // The default size of the buffer to use for downloading Favicons in the event no size is given
    // by the server.
    private static final int DEFAULT_FAVICON_BUFFER_SIZE = 25000;

    private static AtomicInteger mNextFaviconLoadId = new AtomicInteger(0);
    private int mId;
    private String mPageUrl;
    private String mFaviconUrl;
    private OnFaviconLoadedListener mListener;
    private int mFlags;

    private final boolean mOnlyFromLocal;

    // Assuming square favicons, judging by width only is acceptable.
    protected int mTargetWidth;
    private LinkedList<LoadFaviconTask> mChainees;
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
    private LoadFaviconResult loadFaviconFromDb() {
        ContentResolver resolver = sContext.getContentResolver();
        return BrowserDB.getFaviconForFaviconUrl(resolver, mFaviconUrl);
    }

    // Runs in background thread
    private void saveFaviconToDb(final byte[] encodedFavicon) {
        if ((mFlags & FLAG_PERSIST) == 0) {
            return;
        }

        ContentResolver resolver = sContext.getContentResolver();
        BrowserDB.updateFaviconForUrl(resolver, mPageUrl, encodedFavicon, mFaviconUrl);
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

    /**
     * Retrieve the specified favicon from the JAR, returning null if it's not
     * a JAR URI.
     */
    private static Bitmap fetchJARFavicon(String uri) {
        if (uri == null) {
            return null;
        }
        if (uri.startsWith("jar:jar:")) {
            Log.d(LOGTAG, "Fetching favicon from JAR.");
            try {
                return GeckoJarReader.getBitmap(sContext.getResources(), uri);
            } catch (Exception e) {
                // Just about anything could happen here.
                Log.w(LOGTAG, "Error fetching favicon from JAR.", e);
                return null;
            }
        }
        return null;
    }

    // Runs in background thread.
    // Does not attempt to fetch from JARs.
    private LoadFaviconResult downloadFavicon(URI targetFaviconURI) {
        if (targetFaviconURI == null) {
            return null;
        }

        // Only get favicons for HTTP/HTTPS.
        String scheme = targetFaviconURI.getScheme();
        if (!"http".equals(scheme) && !"https".equals(scheme)) {
            return null;
        }

        LoadFaviconResult result = null;

        try {
            result = downloadAndDecodeImage(targetFaviconURI);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error reading favicon", e);
        }

        return result;
    }

    /**
     * Download the Favicon from the given URL and pass it to the decoder function.
     *
     * @param targetFaviconURL URL of the favicon to download.
     * @return A LoadFaviconResult containing the bitmap(s) extracted from the downloaded file, or
     *         null if no or corrupt data ware received.
     * @throws IOException If attempts to fully read the stream result in such an exception, such as
     *                     in the event of a transient connection failure.
     * @throws URISyntaxException If the underlying call to tryDownload retries and raises such an
     *                            exception trying a fallback URL.
     */
    private LoadFaviconResult downloadAndDecodeImage(URI targetFaviconURL) throws IOException, URISyntaxException {
        // Try the URL we were given.
        HttpResponse response = tryDownload(targetFaviconURL);
        if (response == null) {
            return null;
        }

        HttpEntity entity = response.getEntity();
        if (entity == null) {
            return null;
        }

        // This may not be provided, but if it is, it's useful.
        final long entityReportedLength = entity.getContentLength();
        int bufferSize;
        if (entityReportedLength > 0) {
            // The size was reported and sane, so let's use that.
            // Integer overflow should not be a problem for Favicon sizes...
            bufferSize = (int) entityReportedLength + 1;
        } else {
            // No declared size, so guess and reallocate later if it turns out to be too small.
            bufferSize = DEFAULT_FAVICON_BUFFER_SIZE;
        }

        // Allocate a buffer to hold the raw favicon data downloaded.
        byte[] buffer = new byte[bufferSize];

        // The offset of the start of the buffer's free space.
        int bPointer = 0;

        // The quantity of bytes the last call to read yielded.
        int lastRead = 0;
        InputStream contentStream = entity.getContent();
        try {
            // Fully read the entity into the buffer - decoding of streams is not supported
            // (and questionably pointful - what would one do with a half-decoded Favicon?)
            while (lastRead != -1) {
                // Read as many bytes as are currently available into the buffer.
                lastRead = contentStream.read(buffer, bPointer, buffer.length - bPointer);
                bPointer += lastRead;

                // If buffer has overflowed, double its size and carry on.
                if (bPointer == buffer.length) {
                    bufferSize *= 2;
                    byte[] newBuffer = new byte[bufferSize];

                    // Copy the contents of the old buffer into the new buffer.
                    System.arraycopy(buffer, 0, newBuffer, 0, buffer.length);
                    buffer = newBuffer;
                }
            }
        } finally {
            contentStream.close();
        }

        // Having downloaded the image, decode it.
        return FaviconDecoder.decodeFavicon(buffer, 0, bPointer + 1);
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
                storedFaviconUrl = Favicons.getFaviconURLForPageURL(mPageUrl);
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

                if (TextUtils.isEmpty(mFaviconUrl)) {
                    return null;
                }
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

                // If we are chaining, we want to keep the first task started to do this job as the one
                // in the hashmap so subsequent tasks will add themselves to its chaining list.
                return null;
            }

            // We do not want to update the hashmap if the task has chained - other tasks need to
            // chain onto the same parent task.
            loadsInFlight.put(mFaviconUrl, this);
        }

        if (isCancelled()) {
            return null;
        }

        // If there are no valid bitmaps decoded, the returned LoadFaviconResult is null.
        LoadFaviconResult loadedBitmaps = loadFaviconFromDb();
        if (loadedBitmaps != null) {
            return pushToCacheAndGetResult(loadedBitmaps);
        }

        if (mOnlyFromLocal || isCancelled()) {
            return null;
        }

        // Let's see if it's in a JAR.
        image = fetchJARFavicon(mFaviconUrl);
        if (imageIsValid(image)) {
            // We don't want to put this into the DB.
            Favicons.putFaviconInMemCache(mFaviconUrl, image);
            return image;
        }

        try {
            loadedBitmaps = downloadFavicon(new URI(mFaviconUrl));
        } catch (URISyntaxException e) {
            Log.e(LOGTAG, "The provided favicon URL is not valid");
            return null;
        } catch (Exception e) {
            Log.e(LOGTAG, "Couldn't download favicon.", e);
        }

        if (loadedBitmaps != null) {
            saveFaviconToDb(loadedBitmaps.getBytesForDatabaseStorage());
            return pushToCacheAndGetResult(loadedBitmaps);
        }

        if (isUsingDefaultURL) {
            Favicons.putFaviconInFailedCache(mFaviconUrl);
            return null;
        }

        if (isCancelled()) {
            return null;
        }

        // If we're not already trying the default URL, try it now.
        final String guessed = Favicons.guessDefaultFaviconURL(mPageUrl);
        if (guessed == null) {
            Favicons.putFaviconInFailedCache(mFaviconUrl);
            return null;
        }

        image = fetchJARFavicon(guessed);
        if (imageIsValid(image)) {
            // We don't want to put this into the DB.
            Favicons.putFaviconInMemCache(mFaviconUrl, image);
            return image;
        }

        try {
            loadedBitmaps = downloadFavicon(new URI(guessed));
        } catch (Exception e) {
            // Not interesting. It was an educated guess, anyway.
            return null;
        }

        if (loadedBitmaps != null) {
            saveFaviconToDb(loadedBitmaps.getBytesForDatabaseStorage());
            return pushToCacheAndGetResult(loadedBitmaps);
        }

        return null;
    }

    /**
     * Helper method to put the result of a favicon load into the memory cache and then query the
     * cache for the particular bitmap we want for this request.
     * This call is certain to succeed, provided there was enough memory to decode this favicon.
     *
     * @param loadedBitmaps LoadFaviconResult to store.
     * @return The optimal favicon available to satisfy this LoadFaviconTask's request, or null if
     *         we are under extreme memory pressure and find ourselves dropping the cache immediately.
     */
    private Bitmap pushToCacheAndGetResult(LoadFaviconResult loadedBitmaps) {
        Favicons.putFaviconsInMemCache(mFaviconUrl, loadedBitmaps.getBitmaps());
        Bitmap result = Favicons.getSizedFaviconFromCache(mFaviconUrl, mTargetWidth);
        return result;
    }

    private static boolean imageIsValid(final Bitmap image) {
        return image != null &&
               image.getWidth() > 0 &&
               image.getHeight() > 0;
    }

    @Override
    protected void onPostExecute(Bitmap image) {
        if (mIsChaining) {
            return;
        }

        // Process the result, scale for the listener, etc.
        processResult(image);

        synchronized (loadsInFlight) {
            // Prevent any other tasks from chaining on this one.
            loadsInFlight.remove(mFaviconUrl);
        }

        // Since any update to mChainees is done while holding the loadsInFlight lock, once we reach
        // this point no further updates to that list can possibly take place (As far as other tasks
        // are concerned, there is no longer a task to chain from. The above block will have waited
        // for any tasks that were adding themselves to the list before reaching this point.)

        // As such, I believe we're safe to do the following without holding the lock.
        // This is nice - we do not want to take the lock unless we have to anyway, and chaining rarely
        // actually happens outside of the strange situations unit tests create.

        // Share the result with all chained tasks.
        if (mChainees != null) {
            for (LoadFaviconTask t : mChainees) {
                // In the case that we just decoded multiple favicons, either we're passing the right
                // image now, or the call into the cache in processResult will fetch the right one.
                t.processResult(image);
            }
        }
    }

    private void processResult(Bitmap image) {
        Favicons.removeLoadTask(mId);
        Bitmap scaled = image;

        // Notify listeners, scaling if required.
        if (mTargetWidth != -1 && image != null &&  image.getWidth() != mTargetWidth) {
            scaled = Favicons.getSizedFaviconFromCache(mFaviconUrl, mTargetWidth);
        }

        Favicons.dispatchResult(mPageUrl, mFaviconUrl, scaled, mListener);
    }

    @Override
    protected void onCancelled() {
        Favicons.removeLoadTask(mId);

        synchronized(loadsInFlight) {
            // Only remove from the hashmap if the task there is the one that's being canceled.
            // Cancellation of a task that would have chained is not interesting to the hashmap.
            final LoadFaviconTask primary = loadsInFlight.get(mFaviconUrl);
            if (primary == this) {
                loadsInFlight.remove(mFaviconUrl);
                return;
            }
            if (primary == null) {
                // This shouldn't happen.
                return;
            }
            if (primary.mChainees != null) {
              primary.mChainees.remove(this);
            }
        }

        // Note that we don't call the listener callback if the
        // favicon load is cancelled.
    }

    /**
     * When the result of this job is ready, also notify the chainee of the result.
     * Used for aggregating concurrent requests for the same Favicon into a single actual request.
     * (Don't want to download a hundred instances of Google's Favicon at once, for example).
     * The loadsInFlight lock must be held when calling this function.
     *
     * @param aChainee LoadFaviconTask
     */
    private void chainTasks(LoadFaviconTask aChainee) {
        if (mChainees == null) {
            mChainees = new LinkedList<LoadFaviconTask>();
        }

        mChainees.add(aChainee);
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
