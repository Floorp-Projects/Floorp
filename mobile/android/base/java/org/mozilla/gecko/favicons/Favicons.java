/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons;

import android.graphics.drawable.Drawable;
import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.URLMetadataTable;
import org.mozilla.gecko.favicons.cache.FaviconCache;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.NonEvictingLruCache;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public class Favicons {
    private static final String LOGTAG = "GeckoFavicons";

    public enum LoadType {
        PRIVILEGED,
        UNPRIVILEGED
    }

    // A magic URL representing the app's own favicon, used for about: pages.
    private static final String BUILT_IN_FAVICON_URL = "about:favicon";

    // A magic URL representing the app's search favicon, used for about:home.
    private static final String BUILT_IN_SEARCH_URL = "about:search";

    // Size of the favicon bitmap cache, in bytes (Counting payload only).
    public static final int FAVICON_CACHE_SIZE_BYTES = 512 * 1024;

    // Number of URL mappings from page URL to Favicon URL to cache in memory.
    public static final int NUM_PAGE_URL_MAPPINGS_TO_STORE = 128;

    public static final int NOT_LOADING  = 0;
    public static final int LOADED       = 1;

    // The default Favicon to show if no other can be found.
    public static Bitmap defaultFavicon;

    // The density-adjusted default Favicon dimensions.
    public static int defaultFaviconSize;

    // The density-adjusted maximum Favicon dimensions.
    public static int largestFaviconSize;

    // The density-adjusted desired size for a browser-toolbar favicon.
    public static int browserToolbarFaviconSize;

    // Used to prevent multiple-initialisation.
    public static final AtomicBoolean isInitialized = new AtomicBoolean(false);

    // Executor for long-running Favicon Tasks.
    public static final ExecutorService longRunningExecutor = Executors.newSingleThreadExecutor();

    private static final SparseArray<LoadFaviconTask> loadTasks = new SparseArray<>();

    // Cache to hold mappings between page URLs and Favicon URLs. Used to avoid going to the DB when
    // doing so is not necessary.
    private static final NonEvictingLruCache<String, String> pageURLMappings = new NonEvictingLruCache<>(NUM_PAGE_URL_MAPPINGS_TO_STORE);

    // Mime types of things we are capable of decoding.
    private static final HashSet<String> sDecodableMimeTypes = new HashSet<>();

    // Mime types of things we are both capable of decoding and are container formats (May contain
    // multiple different sizes of image)
    private static final HashSet<String> sContainerMimeTypes = new HashSet<>();
    static {
        // MIME types extracted from http://filext.com - ostensibly all in-use mime types for the
        // corresponding formats.
        // ICO
        sContainerMimeTypes.add("image/vnd.microsoft.icon");
        sContainerMimeTypes.add("image/ico");
        sContainerMimeTypes.add("image/icon");
        sContainerMimeTypes.add("image/x-icon");
        sContainerMimeTypes.add("text/ico");
        sContainerMimeTypes.add("application/ico");

        // Add supported container types to the set of supported types.
        sDecodableMimeTypes.addAll(sContainerMimeTypes);

        // PNG
        sDecodableMimeTypes.add("image/png");
        sDecodableMimeTypes.add("application/png");
        sDecodableMimeTypes.add("application/x-png");

        // GIF
        sDecodableMimeTypes.add("image/gif");

        // JPEG
        sDecodableMimeTypes.add("image/jpeg");
        sDecodableMimeTypes.add("image/jpg");
        sDecodableMimeTypes.add("image/pipeg");
        sDecodableMimeTypes.add("image/vnd.swiftview-jpeg");
        sDecodableMimeTypes.add("application/jpg");
        sDecodableMimeTypes.add("application/x-jpg");

        // BMP
        sDecodableMimeTypes.add("application/bmp");
        sDecodableMimeTypes.add("application/x-bmp");
        sDecodableMimeTypes.add("application/x-win-bitmap");
        sDecodableMimeTypes.add("image/bmp");
        sDecodableMimeTypes.add("image/x-bmp");
        sDecodableMimeTypes.add("image/x-bitmap");
        sDecodableMimeTypes.add("image/x-xbitmap");
        sDecodableMimeTypes.add("image/x-win-bitmap");
        sDecodableMimeTypes.add("image/x-windows-bitmap");
        sDecodableMimeTypes.add("image/x-ms-bitmap");
        sDecodableMimeTypes.add("image/x-ms-bmp");
        sDecodableMimeTypes.add("image/ms-bmp");
    }

    public static String getFaviconURLForPageURLFromCache(String pageURL) {
        return pageURLMappings.get(pageURL);
    }

    /**
     * Insert the given pageUrl->faviconUrl mapping into the memory cache of such mappings.
     * Useful for short-circuiting local database access.
     */
    public static void putFaviconURLForPageURLInCache(String pageURL, String faviconURL) {
        pageURLMappings.put(pageURL, faviconURL);
    }

    private static FaviconCache faviconsCache;

    /**
     * Select the closest icon size from a list of icon sizes.
     * We just find the first icon that is larger than the preferred size if available, or otherwise select the
     * largest icon (if all icons are smaller than the preferred size).
     *
     * @return The closes icon size, or -1 if no sizes are supplied.
     */
    public static int selectBestSizeFromList(final List<Integer> sizes, final int preferredSize) {
        Collections.sort(sizes);

        for (int size : sizes) {
            if (size >= preferredSize) {
                return size;
            }
        }

        // If all icons are smaller than the preferred size then we don't have an icon
        // selected yet, therefore just take the largest (last) icon.
        if (sizes.size() > 0) {
            return sizes.get(sizes.size() - 1);
        } else {
            // This isn't ideal, however current code assumes this as an error value for now.
            return -1;
        }
    }

    /**
     * Returns either NOT_LOADING, or LOADED if the onFaviconLoaded call could
     * be made on the main thread.
     * If no listener is provided, NOT_LOADING is returned.
     */
    static int dispatchResult(final String pageUrl, final String faviconURL, final Bitmap image,
            final OnFaviconLoadedListener listener) {
        if (listener == null) {
            return NOT_LOADING;
        }

        if (ThreadUtils.isOnUiThread()) {
            listener.onFaviconLoaded(pageUrl, faviconURL, image);
            return LOADED;
        }

        // We want to always run the listener on UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                listener.onFaviconLoaded(pageUrl, faviconURL, image);
            }
        });
        return NOT_LOADING;
    }

    /**
     * Get a Favicon as close as possible to the target dimensions for the URL provided.
     * If a result is instantly available from the cache, it is returned and the listener is invoked.
     * Otherwise, the result is drawn from the database or network and the listener invoked when the
     * result becomes available.
     *
     * @param pageURL Page URL for which a Favicon is desired.
     * @param faviconURL URL of the Favicon to be downloaded, if known. If none provided, an educated
     *                    guess is made by the system.
     * @param targetSize Target size of the returned Favicon
     * @param listener Listener to call with the result of the load operation, if the result is not
     *                  immediately available.
     * @return The id of the asynchronous task created, NOT_LOADING if none is created, or
     *         LOADED if the value could be dispatched on the current thread.
     */
    public static int getSizedFavicon(Context context, String pageURL, String faviconURL,
                                      LoadType loadType, int targetSize, int flags, OnFaviconLoadedListener listener) {
        // Do we know the favicon URL for this page already?
        String cacheURL = faviconURL;
        if (cacheURL == null) {
            cacheURL = pageURLMappings.get(pageURL);
        }

        // If there's no favicon URL given, try and hit the cache with the default one.
        if (cacheURL == null) {
            cacheURL = guessDefaultFaviconURL(pageURL);
        }

        if (cacheURL == null) {
            // If it's something we can't even figure out a default URL for, just give up.
            return dispatchResult(pageURL, null, defaultFavicon, listener);
        } else if (loadType != LoadType.PRIVILEGED &&
                !(cacheURL.startsWith("http://") || cacheURL.startsWith("https://"))) {
            // Don't load internal / other favicons for non-privileged pages. This is only relevant
            // for getSizedFavicon since this is the only method that allows using a specific favicon
            // URL. All other methods operate via the cache, icons will only end up in the cache
            // if we load them via getSizedFavicon in the first place.
            return NOT_LOADING;
        }

        Bitmap cachedIcon = getSizedFaviconFromCache(cacheURL, targetSize);
        if (cachedIcon != null) {
            return dispatchResult(pageURL, cacheURL, cachedIcon, listener);
        }

        // Check if favicon has failed.
        if (faviconsCache.isFailedFavicon(cacheURL)) {
            return dispatchResult(pageURL, cacheURL, defaultFavicon, listener);
        }

        // Failing that, try and get one from the database or internet.
        return loadUncachedFavicon(context, pageURL, faviconURL, flags, targetSize, listener);
    }

    /**
     * Returns the cached Favicon closest to the target size if any exists or is coercible. Returns
     * null otherwise. Does not query the database or network for the Favicon is the result is not
     * immediately available.
     *
     * @param faviconURL URL of the Favicon to query for.
     * @param targetSize The desired size of the returned Favicon.
     * @return The cached Favicon, rescaled to be as close as possible to the target size, if any exists.
     *         null if no applicable Favicon exists in the cache.
     */
    static Bitmap getSizedFaviconFromCache(String faviconURL, int targetSize) {
        return faviconsCache.getFaviconForDimensions(faviconURL, targetSize);
    }

    /**
     * Attempts to find a Favicon for the provided page URL from either the mem cache or the database.
     * Does not need an explicit favicon URL, since, as we are accessing the database anyway, we
     * can query the history DB for the Favicon URL.
     * Handy for easing the transition from caching with page URLs to caching with Favicon URLs.
     *
     * A null result is passed to the listener if no value is locally available. The Favicon is not
     * added to the failure cache.
     *
     * @param pageURL Page URL for which a Favicon is wanted.
     * @param targetSize Target size of the desired Favicon to pass to the cache query
     * @param callback Callback to fire with the result.
     * @return The job ID of the spawned async task, if any.
     */
    public static int getSizedFaviconForPageFromLocal(Context context, final String pageURL, final int targetSize,
                                                      final OnFaviconLoadedListener callback) {
        // Firstly, try extremely hard to cheat.
        // Have we cached this favicon URL? If we did, we can consult the memcache right away.
        final String targetURL = pageURLMappings.get(pageURL);
        if (targetURL != null) {
            // Check if favicon has failed.
            if (faviconsCache.isFailedFavicon(targetURL)) {
                return dispatchResult(pageURL, targetURL, null, callback);
            }

            // Do we have a Favicon in the cache for this favicon URL?
            final Bitmap result = getSizedFaviconFromCache(targetURL, targetSize);
            if (result != null) {
                // Victory - immediate response!
                return dispatchResult(pageURL, targetURL, result, callback);
            }
        }

        // No joy using in-memory resources. Go to background thread and ask the database.
        // Note: this is a near duplicate of loadUncachedFavicon, however loadUncachedFavicon
        // can download favicons, whereas we want to restrict ourselves to the cache.
        final LoadFaviconTask task =
            new LoadFaviconTask(context, pageURL, targetURL, 0, callback, targetSize, /* onlyFromLocal: */ true);
        final int taskId = task.getId();
        synchronized (loadTasks) {
            loadTasks.put(taskId, task);
        }
        task.execute();

        return taskId;
    }

    public static int getSizedFaviconForPageFromLocal(Context context, final String pageURL, final OnFaviconLoadedListener callback) {
        return getSizedFaviconForPageFromLocal(context, pageURL, defaultFaviconSize, callback);
    }

    /**
     * Helper method to determine the URL of the Favicon image for a given page URL by querying the
     * history database. Should only be called from the background thread - does database access.
     *
     * @param db The LocalBrowserDB to use when accessing favicons.
     * @param cr A ContentResolver to run queries through.
     * @param pageURL The URL of a webpage with a Favicon.
     * @return The URL of the Favicon used by that webpage, according to either the History database
     *         or a somewhat educated guess.
     */
    public static String getFaviconURLForPageURL(final BrowserDB db, final ContentResolver cr, final String pageURL) {
        // Attempt to determine the Favicon URL from the Tabs datastructure. Can dodge having to use
        // the database sometimes by doing this.
        String targetURL;
        Tab theTab = Tabs.getInstance().getFirstTabForUrl(pageURL);
        if (theTab != null) {
            targetURL = theTab.getFaviconURL();
            if (targetURL != null) {
                return targetURL;
            }
        }

        // Try to find the faviconURL in the history and/or bookmarks table.
        targetURL = db.getFaviconURLFromPageURL(cr, pageURL);
        if (targetURL != null) {
            return targetURL;
        }

        // If we still can't find it, fall back to the default URL and hope for the best.
        return guessDefaultFaviconURL(pageURL);
    }

    /**
     * Helper function to create an async job to load a Favicon which does not exist in the memcache.
     * Contains logic to prevent the repeated loading of Favicons which have previously failed.
     * There is no support for recovery from transient failures.
     *
     * @param pageURL URL of the page for which to load a Favicon. If null, no job is created.
     * @param faviconURL The URL of the Favicon to load. If null, an attempt to infer the value from
     *                   the history database will be made, and ultimately an attempt to guess will
     *                   be made.
     * @param flags Flags to be used by the LoadFaviconTask while loading. Currently only one flag
     *              is supported, LoadFaviconTask.FLAG_PERSIST.
     *              If FLAG_PERSIST is set and the Favicon is ultimately loaded from the internet,
     *              the downloaded Favicon is subsequently stored in the local database.
     *              If FLAG_PERSIST is unset, the downloaded Favicon is stored only in the memcache.
     *              FLAG_PERSIST has no effect on loads which come from the database.
     * @param listener The OnFaviconLoadedListener to invoke with the result of this Favicon load.
     * @return The id of the LoadFaviconTask handling this job.
     */
    private static int loadUncachedFavicon(Context context, String pageURL, String faviconURL, int flags,
                                           int targetSize, OnFaviconLoadedListener listener) {
        // Handle the case where we have no page url.
        if (TextUtils.isEmpty(pageURL)) {
            dispatchResult(null, null, null, listener);
            return NOT_LOADING;
        }

        final LoadFaviconTask task =
            new LoadFaviconTask(context, pageURL, faviconURL, flags, listener, targetSize, false);
        final int taskId = task.getId();
        synchronized (loadTasks) {
            loadTasks.put(taskId, task);
        }
        task.execute();

        return taskId;
    }

    public static void putFaviconInMemCache(String pageUrl, Bitmap image) {
        faviconsCache.putSingleFavicon(pageUrl, image);
    }

    /**
     * Adds the bitmaps given by the specified iterator to the cache associated with the url given.
     * Future requests for images will be able to select the least larger image than the target
     * size from this new set of images.
     *
     * @param pageUrl The URL to associate the new favicons with.
     * @param images An iterator over the new favicons to put in the cache.
     */
    public static void putFaviconsInMemCache(String pageUrl, Iterator<Bitmap> images, boolean permanently) {
        faviconsCache.putFavicons(pageUrl, images, permanently);
    }

    public static void putFaviconsInMemCache(String pageUrl, Iterator<Bitmap> images) {
        putFaviconsInMemCache(pageUrl, images, false);
    }

    public static void clearMemCache() {
        faviconsCache.evictAll();
        pageURLMappings.evictAll();
    }

    public static void putFaviconInFailedCache(String faviconURL) {
        faviconsCache.putFailed(faviconURL);
    }

    public static boolean cancelFaviconLoad(int taskId) {
        if (taskId == NOT_LOADING) {
            return false;
        }

        synchronized (loadTasks) {
            if (loadTasks.indexOfKey(taskId) < 0) {
                return false;
            }

            Log.v(LOGTAG, "Cancelling favicon load " + taskId + ".");
            LoadFaviconTask task = loadTasks.get(taskId);
            return task.cancel();
        }
    }

    public static void close() {
        Log.d(LOGTAG, "Closing Favicons database");

        // Close the Executor to new tasks.
        longRunningExecutor.shutdown();

        // Cancel any pending tasks
        synchronized (loadTasks) {
            final int count = loadTasks.size();
            for (int i = 0; i < count; i++) {
                cancelFaviconLoad(loadTasks.keyAt(i));
            }
            loadTasks.clear();
        }
    }

    /**
     * Get the dominant colour of the Favicon at the URL given, if any exists in the cache.
     *
     * @param url The URL of the Favicon, to be used as the cache key for the colour value.
     * @return The dominant colour of the provided Favicon.
     */
    public static int getFaviconColor(String url) {
        return faviconsCache.getDominantColor(url);
    }

    /**
     * Called by GeckoApp on startup to pass this class a reference to the GeckoApp object used as
     * the application's Context.
     * Consider replacing with references to a staticly held reference to the GeckoApp object.
     *
     * @param context A reference to the GeckoApp instance.
     */
    public static void initializeWithContext(Context context) throws IllegalStateException {
        // Prevent multiple-initialisation.
        if (!isInitialized.compareAndSet(false, true)) {
            return;
        }

        final Resources res = context.getResources();

        final Drawable defaultFaviconDrawable = res.getDrawable(R.drawable.toolbar_favicon_default);
        if (defaultFaviconDrawable instanceof BitmapDrawable) {
            defaultFavicon = ((BitmapDrawable) defaultFaviconDrawable).getBitmap();
        } else {
            throw new IllegalStateException("toolbar_favicon_default wasn't a bitmap resource!");
        }

        defaultFaviconSize = res.getDimensionPixelSize(R.dimen.favicon_bg);

        // Screen-density-adjusted upper limit on favicon size. Favicons larger than this are
        // downscaled to this size or discarded.
        largestFaviconSize = res.getDimensionPixelSize(R.dimen.favicon_largest_interesting_size);

        browserToolbarFaviconSize = res.getDimensionPixelSize(R.dimen.browser_toolbar_favicon_size);

        faviconsCache = new FaviconCache(FAVICON_CACHE_SIZE_BYTES, largestFaviconSize);

        // Initialize page mappings for each of our special pages.
        for (String url : AboutPages.getDefaultIconPages()) {
            pageURLMappings.putWithoutEviction(url, BUILT_IN_FAVICON_URL);
        }

        // Load and cache the built-in favicon in each of its sizes.
        // TODO: don't open the zip twice!
        List<Bitmap> toInsert = Arrays.asList(loadBrandingBitmap(context, "favicon64.png"),
                                              loadBrandingBitmap(context, "favicon32.png"));

        putFaviconsInMemCache(BUILT_IN_FAVICON_URL, toInsert.iterator(), true);

        pageURLMappings.putWithoutEviction(AboutPages.HOME, BUILT_IN_SEARCH_URL);
        List<Bitmap> searchIcons = Collections.singletonList(BitmapFactory.decodeResource(res, R.drawable.search_icon_inactive));
        putFaviconsInMemCache(BUILT_IN_SEARCH_URL, searchIcons.iterator(), true);
    }

    /**
     * Compute a string like:
     * "jar:jar:file:///data/app/org.mozilla.firefox-1.apk!/assets/omni.ja!/chrome/chrome/content/branding/favicon64.png"
     */
    private static String getBrandingBitmapPath(Context context, String name) {
        return GeckoJarReader.getJarURL(context, "chrome/chrome/content/branding/" + name);
    }

    private static Bitmap loadBrandingBitmap(Context context, String name) {
        Bitmap b = GeckoJarReader.getBitmap(context,
                                            context.getResources(),
                                            getBrandingBitmapPath(context, name));
        if (b == null) {
            throw new IllegalStateException("Bitmap " + name + " missing from JAR!");
        }
        return b;
    }

    /**
     * Helper method to get the default Favicon URL for a given pageURL. Generally: somewhere.com/favicon.ico
     *
     * @param pageURL Page URL for which a default Favicon URL is requested
     * @return The default Favicon URL.
     */
    public static String guessDefaultFaviconURL(String pageURL) {
        // Special-casing for about: pages. The favicon for about:pages which don't provide a link tag
        // is bundled in the database, keyed only by page URL, hence the need to return the page URL
        // here. If the database ever migrates to stop being silly in this way, this can plausibly
        // be removed.
        if (TextUtils.isEmpty(pageURL)) {
            return null;
        }
        if (AboutPages.isAboutPage(pageURL) || pageURL.startsWith("jar:")) {
            return pageURL;
        }

        try {
            // Fall back to trying "someScheme:someDomain.someExtension/favicon.ico".
            Uri u = Uri.parse(pageURL);
            Uri.Builder builder = new Uri.Builder();
            builder.scheme(u.getScheme())
                   .authority(u.getAuthority())
                   .appendPath("favicon.ico");
            return builder.build().toString();
        } catch (Exception e) {
            Log.d(LOGTAG, "Exception getting default favicon URL");
            return null;
        }
    }

    /**
     * Helper function to determine if we can decode a particular mime type.
     * @param imgType Mime type to check for decodability.
     * @return false if the given mime type is certainly not decodable, true if it might be.
     */
    public static boolean canDecodeType(String imgType) {
        return "".equals(imgType) || sDecodableMimeTypes.contains(imgType);
    }

    /**
     * Helper function to determine if the provided mime type is that of a format that can contain
     * multiple image types. At time of writing, the only such type is ICO.
     * @param mimeType Mime type to check.
     * @return true if the given mime type is a container type, false otherwise.
     */
    public static boolean isContainerType(String mimeType) {
        return sDecodableMimeTypes.contains(mimeType);
    }

    public static void removeLoadTask(int taskId) {
        synchronized (loadTasks) {
            loadTasks.delete(taskId);
        }
    }

    /**
     * Method to wrap FaviconCache.isFailedFavicon for use by LoadFaviconTask.
     *
     * @param faviconURL Favicon URL to check for failure.
     */
    static boolean isFailedFavicon(String faviconURL) {
        return faviconsCache.isFailedFavicon(faviconURL);
    }

    /**
     * Sidestep the cache and get, from either the database or the internet, a favicon
     * suitable for use as an app icon for the provided URL.
     *
     * Useful for creating homescreen shortcuts without being limited
     * by possibly low-resolution values in the cache.
     *
     * The icon will be scaled to the preferred Android launcher icon size.
     *
     * Deduces the favicon URL from the browser database, guessing if necessary.
     *
     * @param url page URL to get a large favicon image for.
     * @param onFaviconLoadedListener listener to call back with the result.
     */
    private static void getPreferredSizeFaviconForPage(Context context, String url, String iconURL, OnFaviconLoadedListener onFaviconLoadedListener) {
        int preferredSize = GeckoAppShell.getPreferredIconSize();
        loadUncachedFavicon(context, url, iconURL, LoadFaviconTask.FLAG_BYPASS_CACHE_WHEN_DOWNLOADING_ICONS, preferredSize, onFaviconLoadedListener);
    }

    /**
     * Load the icon that is the most suitable for using as a home screen shortcut.
     *
     * This method will try to load a 'touch icon' first. If not available it will fallback to use
     * the best available favicon.
     *
     * This implementation sidesteps the cache and will load the icon from the database or the
     * internet. See getPreferredSizeFaviconForPage().
     */
    public static void getPreferredIconForHomeScreenShortcut(Context context, String url, OnFaviconLoadedListener onFaviconLoadedListener) {
        ThreadUtils.assertOnBackgroundThread();

        final BrowserDB db = GeckoProfile.get(context).getDB();

        final String metadataQueryURL = StringUtils.stripRef(url);

        final ContentResolver cr = context.getContentResolver();
        final Map<String, Map<String, Object>> metadata = db.getURLMetadata().getForURLs(cr,
                Collections.singletonList(metadataQueryURL),
                Collections.singletonList(URLMetadataTable.TOUCH_ICON_COLUMN)
        );

        final Map<String, Object> row = metadata.get(metadataQueryURL);

        String touchIconURL = null;

        if (row != null) {
            touchIconURL = (String) row.get(URLMetadataTable.TOUCH_ICON_COLUMN);
        }

        if (touchIconURL != null &&
            !(touchIconURL.startsWith("http://") || touchIconURL.startsWith("https://"))) {
            // We definitely don't want to load internal icons for homescreen shortcuts. See
            // our use of LoadType.PRIVILEGED above for where allow non http(s) icons
            touchIconURL = null;
        }

        // Retrieve the icon while bypassing the cache. Homescreen icon creation is a one-off event, hence it isn't
        // useful to cache these icons. (Android takes care of storing homescreen icons after a shortcut
        // has been created.)
        // The cache is also (currently) limited to 32dp, hence we explicitly need to avoid accessing those icons.
        // If touchIconURL is null, then Favicons falls back to finding the best possible favicon for
        // the site URI, hence we can use this call even when there is no touchIcon defined.
        getPreferredSizeFaviconForPage(context, url, touchIconURL, onFaviconLoadedListener);
    }
}
