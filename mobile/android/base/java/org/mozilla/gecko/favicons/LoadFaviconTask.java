/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.Log;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.favicons.decoders.FaviconDecoder;
import org.mozilla.gecko.favicons.decoders.LoadFaviconResult;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.ProxySelector;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

import static org.mozilla.gecko.util.IOUtils.ConsumedInputStream;

/**
 * Class representing the asynchronous task to load a Favicon which is not currently in the in-memory
 * cache.
 * The implementation initially tries to get the Favicon from the database. Upon failure, the icon
 * is loaded from the internet.
 */
public class LoadFaviconTask {
    private static final String LOGTAG = "LoadFaviconTask";

    // Access to this map needs to be synchronized prevent multiple jobs loading the same favicon
    // from executing concurrently.
    private static final HashMap<String, LoadFaviconTask> loadsInFlight = new HashMap<>();

    public static final int FLAG_PERSIST = 1;
    /**
     * Bypass all caches - this is used to directly retrieve the requested icon. Without this flag,
     * favicons will first be pushed into the memory cache (and possibly permanent cache if using FLAG_PERSIST),
     * where they will be downscaled to the maximum cache size, before being retrieved from the cache (resulting
     * in a possibly smaller icon size).
     */
    public static final int FLAG_BYPASS_CACHE_WHEN_DOWNLOADING_ICONS = 2;

    private static final int MAX_REDIRECTS_TO_FOLLOW = 5;
    // The default size of the buffer to use for downloading Favicons in the event no size is given
    // by the server.
    public static final int DEFAULT_FAVICON_BUFFER_SIZE = 25000;

    private static final AtomicInteger nextFaviconLoadId = new AtomicInteger(0);
    private final Context context;
    private final int id;
    private final String pageUrl;
    private String faviconURL;
    private final OnFaviconLoadedListener listener;
    private final int flags;
    private final BrowserDB db;

    private final boolean onlyFromLocal;
    volatile boolean mCancelled;

    // Assuming square favicons, judging by width only is acceptable.
    protected int targetWidthAndHeight;
    private LinkedList<LoadFaviconTask> chainees;
    private boolean isChaining;

    private static class Response {
        public final int contentLength;
        public final InputStream stream;

        private Response(InputStream stream, int contentLength) {
            this.stream = stream;
            this.contentLength = contentLength;
        }
    }

    public LoadFaviconTask(Context context, String pageURL, String faviconURL, int flags, OnFaviconLoadedListener listener,
                           int targetWidth, boolean onlyFromLocal) {
        id = nextFaviconLoadId.incrementAndGet();

        this.context = context;
        db = GeckoProfile.get(context).getDB();
        this.pageUrl = pageURL;
        this.faviconURL = faviconURL;
        this.listener = listener;
        this.flags = flags;
        this.targetWidthAndHeight = targetWidth;
        this.onlyFromLocal = onlyFromLocal;
    }

    // Runs in background thread
    private LoadFaviconResult loadFaviconFromDb(final BrowserDB db) {
        ContentResolver resolver = context.getContentResolver();
        return db.getFaviconForUrl(resolver, faviconURL);
    }

    // Runs in background thread
    private void saveFaviconToDb(final BrowserDB db, final byte[] encodedFavicon) {
        if (encodedFavicon == null) {
            return;
        }

        if ((flags & FLAG_PERSIST) == 0) {
            return;
        }

        ContentResolver resolver = context.getContentResolver();
        db.updateFaviconForUrl(resolver, pageUrl, encodedFavicon, faviconURL);
    }

    /**
     * Helper method for trying the download request to grab a Favicon.
     * @param faviconURI URL of Favicon to try and download
     * @return The HttpResponse containing the downloaded Favicon if successful, null otherwise.
     */
    private Response tryDownload(URI faviconURI) throws URISyntaxException, IOException {
        HashSet<String> visitedLinkSet = new HashSet<>();
        visitedLinkSet.add(faviconURI.toString());
        return tryDownloadRecurse(faviconURI, visitedLinkSet);
    }
    private Response tryDownloadRecurse(URI faviconURI, HashSet<String> visited) throws URISyntaxException, IOException {
        if (visited.size() == MAX_REDIRECTS_TO_FOLLOW) {
            return null;
        }

        HttpURLConnection connection = (HttpURLConnection) ProxySelector.openConnectionWithProxy(faviconURI);
        connection.setRequestProperty("User-Agent", GeckoAppShell.getGeckoInterface().getDefaultUAString());

        connection.connect();

        // Was the response a failure?
        int status = connection.getResponseCode();

        // Handle HTTP status codes requesting a redirect.
        if (status >= 300 && status < 400) {
            final String newURI = connection.getHeaderField("Location");

            // Handle mad webservers.
            try {
                if (newURI == null || newURI.equals(faviconURI.toString())) {
                    return null;
                }

                if (visited.contains(newURI)) {
                    // Already been redirected here - abort.
                    return null;
                }

                visited.add(newURI);
            } finally {
                connection.disconnect();
            }

            return tryDownloadRecurse(new URI(newURI), visited);
        }

        if (status >= 400) {
            connection.disconnect();
            return null;
        }

        return new Response(connection.getInputStream(), connection.getHeaderFieldInt("Content-Length", -1));
    }

    /**
     * Retrieve the specified favicon from the JAR, returning null if it's not
     * a JAR URI.
     */
    private Bitmap fetchJARFavicon(String uri) {
        if (uri == null) {
            return null;
        }
        if (uri.startsWith("jar:jar:")) {
            Log.d(LOGTAG, "Fetching favicon from JAR.");
            try {
                return GeckoJarReader.getBitmap(context, context.getResources(), uri);
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
        } catch (OutOfMemoryError e) {
            Log.e(LOGTAG, "Insufficient memory to process favicon");
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
        Response response = tryDownload(targetFaviconURL);
        if (response == null) {
            return null;
        }

        // Decode the image from the fetched response.
        try {
            return decodeImageFromResponse(response);
        } finally {
            // Close the stream and free related resources.
            IOUtils.safeStreamClose(response.stream);
        }
    }

    /**
     * Copies the favicon stream to a buffer and decodes downloaded content  into bitmaps using the
     * FaviconDecoder.
     *
     * @param response Response containing the favicon stream to decode.
     * @return A LoadFaviconResult containing the bitmap(s) extracted from the downloaded file, or
     *         null if no or corrupt data were received.
     * @throws IOException If attempts to fully read the stream result in such an exception, such as
     *                     in the event of a transient connection failure.
     */
    private LoadFaviconResult decodeImageFromResponse(Response response) throws IOException {
        // This may not be provided, but if it is, it's useful.
        int bufferSize;
        if (response.contentLength > 0) {
            // The size was reported and sane, so let's use that.
            // Integer overflow should not be a problem for Favicon sizes...
            bufferSize = response.contentLength + 1;
        } else {
            // No declared size, so guess and reallocate later if it turns out to be too small.
            bufferSize = DEFAULT_FAVICON_BUFFER_SIZE;
        }

        // Read the InputStream into a byte[].
        ConsumedInputStream result = IOUtils.readFully(response.stream, bufferSize);
        if (result == null) {
            return null;
        }

        // Having downloaded the image, decode it.
        return FaviconDecoder.decodeFavicon(result.getData(), 0, result.consumedLength);
    }

    // LoadFaviconTasks are performed on a unique background executor thread
    // to avoid network blocking.
    public final void execute() {
        try {
            Favicons.longRunningExecutor.execute(new Runnable() {
                @Override
                public void run() {
                    final Bitmap result = doInBackground(db);

                    ThreadUtils.getUiHandler().post(new Runnable() {
                       @Override
                        public void run() {
                            if (mCancelled) {
                                onCancelled();
                            } else {
                                onPostExecute(result);
                            }
                        }
                    });
                }
            });
          } catch (RejectedExecutionException e) {
              // If our executor is unavailable.
              onCancelled();
          }
    }

    public final boolean cancel() {
        mCancelled = true;
        return true;
    }

    public final boolean isCancelled() {
        return mCancelled;
    }

    Bitmap doInBackground(final BrowserDB db) {
        if (isCancelled()) {
            return null;
        }

        // Attempt to decode the favicon URL as a data URL. We don't bother storing such URIs in
        // the database: the cost of decoding them here probably doesn't exceed the cost of mucking
        // about with the DB.
        final boolean isEmpty = TextUtils.isEmpty(faviconURL);
        if (!isEmpty) {
            LoadFaviconResult uriBitmaps = FaviconDecoder.decodeDataURI(faviconURL);
            if (uriBitmaps != null) {
                return pushToCacheAndGetResult(uriBitmaps);
            }
        }

        String storedFaviconUrl;
        boolean isUsingDefaultURL = false;

        // Handle the case of malformed favicon URL.
        // If favicon is empty, fall back to the stored one.
        if (isEmpty) {
            // Try to get the favicon URL from the memory cache.
            final ContentResolver cr = context.getContentResolver();
            storedFaviconUrl = Favicons.getFaviconURLForPageURLFromCache(pageUrl);

            // If that failed, try to get the URL from the database.
            if (storedFaviconUrl == null) {
                storedFaviconUrl = Favicons.getFaviconURLForPageURL(db, cr, pageUrl);
                if (storedFaviconUrl != null) {
                    // If that succeeded, cache the URL loaded from the database in memory.
                    Favicons.putFaviconURLForPageURLInCache(pageUrl, storedFaviconUrl);
                }
            }

            // If we found a faviconURL - use it.
            if (storedFaviconUrl != null) {
                faviconURL = storedFaviconUrl;
            } else {
                // If we don't have a stored one, fall back to the default.
                faviconURL = Favicons.guessDefaultFaviconURL(pageUrl);

                if (TextUtils.isEmpty(faviconURL)) {
                    return null;
                }
                isUsingDefaultURL = true;
            }
        }

        // Check if favicon has failed - if so, give up. We need this check because, sometimes, we
        // didn't know the real Favicon URL until we asked the database.
        if (Favicons.isFailedFavicon(faviconURL)) {
            return null;
        }

        if (isCancelled()) {
            return null;
        }

        Bitmap image;
        // Determine if there is already an ongoing task to fetch the Favicon we desire.
        // If there is, just join the queue and wait for it to finish. If not, we carry on.
        synchronized (loadsInFlight) {
            // Another load of the current Favicon is already underway
            LoadFaviconTask existingTask = loadsInFlight.get(faviconURL);
            if (existingTask != null && !existingTask.isCancelled()) {
                existingTask.chainTasks(this);
                isChaining = true;

                // If we are chaining, we want to keep the first task started to do this job as the one
                // in the hashmap so subsequent tasks will add themselves to its chaining list.
                return null;
            }

            // We do not want to update the hashmap if the task has chained - other tasks need to
            // chain onto the same parent task.
            loadsInFlight.put(faviconURL, this);
        }

        if (isCancelled()) {
            return null;
        }

        LoadFaviconResult loadedBitmaps = null;
        // If there are no valid bitmaps decoded, the returned LoadFaviconResult is null.
        if ((flags & FLAG_BYPASS_CACHE_WHEN_DOWNLOADING_ICONS) == 0) {
            loadedBitmaps = loadFaviconFromDb(db);
            if (loadedBitmaps != null) {
                return pushToCacheAndGetResult(loadedBitmaps);
            }
        }

        if (onlyFromLocal || isCancelled()) {
            return null;
        }

        // Let's see if it's in a JAR.
        image = fetchJARFavicon(faviconURL);
        if (imageIsValid(image)) {
            // We don't want to put this into the DB.
            Favicons.putFaviconInMemCache(faviconURL, image);
            return image;
        }

        try {
            loadedBitmaps = downloadFavicon(new URI(faviconURL));
        } catch (URISyntaxException e) {
            Log.e(LOGTAG, "The provided favicon URL is not valid");
            return null;
        } catch (Exception e) {
            Log.e(LOGTAG, "Couldn't download favicon.", e);
        }

        if (loadedBitmaps != null) {
            if ((flags & FLAG_BYPASS_CACHE_WHEN_DOWNLOADING_ICONS) == 0) {
                // Fetching bytes to store can fail. saveFaviconToDb will
                // do the right thing, but we still choose to cache the
                // downloaded icon in memory.
                saveFaviconToDb(db, loadedBitmaps.getBytesForDatabaseStorage());
                return pushToCacheAndGetResult(loadedBitmaps);
            } else {
                final Map<Integer, Bitmap> iconMap = new HashMap<>();
                final List<Integer> sizes = new ArrayList<>();

                while (loadedBitmaps.getBitmaps().hasNext()) {
                    final Bitmap b = loadedBitmaps.getBitmaps().next();
                    iconMap.put(b.getWidth(), b);
                    sizes.add(b.getWidth());
                }

                int bestSize = Favicons.selectBestSizeFromList(sizes, targetWidthAndHeight);
                return iconMap.get(bestSize);
            }
        }

        if (isUsingDefaultURL) {
            Favicons.putFaviconInFailedCache(faviconURL);
            return null;
        }

        if (isCancelled()) {
            return null;
        }

        // If we're not already trying the default URL, try it now.
        final String guessed = Favicons.guessDefaultFaviconURL(pageUrl);
        if (guessed == null) {
            Favicons.putFaviconInFailedCache(faviconURL);
            return null;
        }

        image = fetchJARFavicon(guessed);
        if (imageIsValid(image)) {
            // We don't want to put this into the DB.
            Favicons.putFaviconInMemCache(faviconURL, image);
            return image;
        }

        try {
            loadedBitmaps = downloadFavicon(new URI(guessed));
        } catch (Exception e) {
            // Not interesting. It was an educated guess, anyway.
            return null;
        }

        if (loadedBitmaps != null) {
            saveFaviconToDb(db, loadedBitmaps.getBytesForDatabaseStorage());
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
        Favicons.putFaviconsInMemCache(faviconURL, loadedBitmaps.getBitmaps());
        return Favicons.getSizedFaviconFromCache(faviconURL, targetWidthAndHeight);
    }

    private static boolean imageIsValid(final Bitmap image) {
        return image != null &&
               image.getWidth() > 0 &&
               image.getHeight() > 0;
    }

    void onPostExecute(Bitmap image) {
        if (isChaining) {
            return;
        }

        // Process the result, scale for the listener, etc.
        processResult(image);

        synchronized (loadsInFlight) {
            // Prevent any other tasks from chaining on this one.
            loadsInFlight.remove(faviconURL);
        }

        // Since any update to chainees is done while holding the loadsInFlight lock, once we reach
        // this point no further updates to that list can possibly take place (As far as other tasks
        // are concerned, there is no longer a task to chain from. The above block will have waited
        // for any tasks that were adding themselves to the list before reaching this point.)

        // As such, I believe we're safe to do the following without holding the lock.
        // This is nice - we do not want to take the lock unless we have to anyway, and chaining rarely
        // actually happens outside of the strange situations unit tests create.

        // Share the result with all chained tasks.
        if (chainees != null) {
            for (LoadFaviconTask t : chainees) {
                // In the case that we just decoded multiple favicons, either we're passing the right
                // image now, or the call into the cache in processResult will fetch the right one.
                t.processResult(image);
            }
        }
    }

    private void processResult(Bitmap image) {
        Favicons.removeLoadTask(id);
        final Bitmap scaled;

        // Notify listeners, scaling if required.
        if (targetWidthAndHeight != -1 && image != null && image.getWidth() != targetWidthAndHeight) {
            if ((flags & FLAG_BYPASS_CACHE_WHEN_DOWNLOADING_ICONS) != 0) {
                scaled = Bitmap.createScaledBitmap(image, targetWidthAndHeight, targetWidthAndHeight, true);
            } else {
                scaled = Favicons.getSizedFaviconFromCache(faviconURL, targetWidthAndHeight);
            }
        } else {
            scaled = image;
        }

        Favicons.dispatchResult(pageUrl, faviconURL, scaled, listener);
    }

    void onCancelled() {
        Favicons.removeLoadTask(id);

        synchronized (loadsInFlight) {
            // Only remove from the hashmap if the task there is the one that's being canceled.
            // Cancellation of a task that would have chained is not interesting to the hashmap.
            final LoadFaviconTask primary = loadsInFlight.get(faviconURL);
            if (primary == this) {
                loadsInFlight.remove(faviconURL);
                return;
            }
            if (primary == null) {
                // This shouldn't happen.
                return;
            }
            if (primary.chainees != null) {
              primary.chainees.remove(this);
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
        if (chainees == null) {
            chainees = new LinkedList<>();
        }

        chainees.add(aChainee);
    }

    int getId() {
        return id;
    }
}
