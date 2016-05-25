/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reader;

import android.content.Context;
import android.support.annotation.NonNull;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;
import java.io.IOException;
import java.util.Iterator;

/**
 * Helper to keep track of items that are stored in the reader view cache. This is an in-memory list
 * of the reader view items that are cached on disk. It is intended to allow quickly determining whether
 * a given URL is in the cache, and also how many cached items there are.
 *
 * Currently we have 1:1 correspondence of reader view items (in the URL-annotations table)
 * to cached items. This is _not_ a true cache, we never purge/cleanup items here - we only remove
 * items when we un-reader-view/bookmark them. This is an acceptable model while we can guarantee the
 * 1:1 correspondence.
 *
 * It isn't strictly necessary to mirror cached items in SQL at this stage, however it seems sensible
 * to maintain URL anotations to avoid additional DB migrations in future.
 * It is also simpler to implement the reading list smart-folder using the annotations (even if we do
 * all other decoration from our in-memory cache record), as that is what we will need when
 * we move away from the 1:1 correspondence.
 *
 * Bookmarks can be in one of two states - plain bookmark, or reader view bookmark that is also saved
 * offline. We're hoping to introduce real cache management / cleanup in future, in which case a
 * third user-visible state (reader view bookmark without a cache entry) will be added. However that logic is
 * much more complicated and requires substantial changes in how we decorate reader view bookmarks.
 * With the current 1:1 correspondence we can use this in-memory helper to quickly decorate
 * bookmarks (in all the various lists and panels that are used), whereas supporting
 * the third state requires significant changes in order to allow joining with the
 * URL-annotations table wherever bookmarks might be retrieved (i.e. multiple homepanels, each with
 * their own loaders and adapter).
 *
 * If/when cache cleanup and sync are implemented, URL annotations will be the canonical record of
 * user intent, and the cache will no longer represent all reader view bookmarks. We will have (A)
 * cached items that are not a bookmark, or bookmarks without the reader view annotation (both of
 * these would need purging), and (B) bookmarks with a reader view annotation, but not stored in
 * the cache (which we might want to download in the background). Supporting (B) is currently difficult,
 * see previous paragraph.
 */
public class SavedReaderViewHelper {
    private static final String LOG_TAG = "SavedReaderViewHelper";

    private static final String PATH = "path";
    private static final String SIZE = "size";

    private static final String DIRECTORY = "readercache";
    private static final String FILE_NAME = "items.json";
    private static final String FILE_PATH = DIRECTORY + "/" + FILE_NAME;

    // We use null to indicate that the cache hasn't yet been loaded. Loading has to be explicitly
    // requested by client code, and must happen on the background thread. Attempting to access
    // items (which happens mainly on the UI thread) before explicitly loading them is not permitted.
    private JSONObject mItems = null;

    private final Context mContext;

    private static SavedReaderViewHelper instance = null;

    private SavedReaderViewHelper(Context context) {
        mContext = context;
    }

    public static synchronized SavedReaderViewHelper getSavedReaderViewHelper(final Context context) {
        if (instance == null) {
            instance = new SavedReaderViewHelper(context);
        }

        return instance;
    }

    /**
     * Load the reader view cache list from our JSON file.
     *
     * Must not be run on the UI thread due to file access.
     */
    public synchronized void loadItems() {
        // TODO bug 1264489
        // This is a band aid fix for Bug 1264134. We need to figure out the root cause and reenable this
        // assertion.
        // ThreadUtils.assertNotOnUiThread();

        if (mItems != null) {
            return;
        }

        try {
            mItems = GeckoProfile.get(mContext).readJSONObjectFromFile(FILE_PATH);
        } catch (IOException e) {
            mItems = new JSONObject();
        }
    }

    private synchronized void assertItemsLoaded() {
        if (mItems == null) {
            throw new IllegalStateException("SavedReaderView items must be explicitly loaded using loadItems() before access.");
        }
    }

    private JSONObject makeItem(@NonNull String path, long size) throws JSONException {
        final JSONObject item = new JSONObject();

        item.put(PATH, path);
        item.put(SIZE, size);

        return item;
    }

    public synchronized boolean isURLCached(@NonNull final String URL) {
        assertItemsLoaded();
        return mItems.has(URL);
    }

    /**
     * Insert an item into the list of cached items.
     *
     * This may be called from any thread.
     */
    public synchronized void put(@NonNull final String pageURL, @NonNull final String path, final long size) {
        assertItemsLoaded();

        try {
            mItems.put(pageURL, makeItem(path, size));
        } catch (JSONException e) {
            Log.w(LOG_TAG, "Item insertion failed:", e);
            // This should never happen, absent any errors in our own implementation
            throw new IllegalStateException("Failure inserting into SavedReaderViewHelper json");
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                UrlAnnotations annotations = GeckoProfile.get(mContext).getDB().getUrlAnnotations();
                annotations.insertReaderViewUrl(mContext.getContentResolver(), pageURL);

                commit();
            }
        });
    }

    protected synchronized void remove(@NonNull final String pageURL) {
        assertItemsLoaded();

        mItems.remove(pageURL);

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                UrlAnnotations annotations = GeckoProfile.get(mContext).getDB().getUrlAnnotations();
                annotations.deleteReaderViewUrl(mContext.getContentResolver(), pageURL);

                commit();
            }
        });
    }

    @RobocopTarget
    public synchronized int size() {
        assertItemsLoaded();
        return mItems.length();
    }

    private synchronized void commit() {
        ThreadUtils.assertOnBackgroundThread();

        GeckoProfile profile = GeckoProfile.get(mContext);
        File cacheDir = new File(profile.getDir(), DIRECTORY);

        if (!cacheDir.exists()) {
            Log.i(LOG_TAG, "No preexisting cache directory, creating now");

            boolean cacheDirCreated = cacheDir.mkdir();
            if (!cacheDirCreated) {
                throw new IllegalStateException("Couldn't create cache directory, unable to track reader view cache");
            }
        }

        profile.writeFile(FILE_PATH, mItems.toString());
    }

    /**
     * Return the Reader View URL for a given URL if it is contained in the cache. Returns the
     * plain URL if the page is not cached.
     */
    public static String getReaderURLIfCached(final Context context, @NonNull final String pageURL) {
        SavedReaderViewHelper rvh = getSavedReaderViewHelper(context);

        if (rvh.isURLCached(pageURL)) {
            return ReaderModeUtils.getAboutReaderForUrl(pageURL);
        } else {
            return pageURL;
        }
    }

    /**
     * Obtain the total disk space used for saved reader view items, in KB.
     *
     * @return Total disk space used (KB), or Integer.MAX_VALUE on overflow.
     */
    public synchronized int getDiskSpacedUsedKB() {
        // JSONObject is not thread safe - we need to be synchronized to avoid issues (most likely to
        // occur if items are removed during iteration).
        final Iterator<String> keys = mItems.keys();
        long bytes = 0;

        while (keys.hasNext()) {
            final String pageURL = keys.next();
            try {
                final JSONObject item = mItems.getJSONObject(pageURL);
                bytes += item.getLong(SIZE);

                // Overflow is highly unlikely (we will hit device storage limits before we hit integer limits),
                // but we should still handle this for correctness.
                // We definitely can't store our output in an int if we overflow the long here.
                if (bytes < 0) {
                    return Integer.MAX_VALUE;
                }
            } catch (JSONException e) {
                // This shouldn't ever happen:
                throw new IllegalStateException("Must be able to access items in saved reader view list", e);
            }
        }

        long kb = bytes / 1024;
        if (kb > Integer.MAX_VALUE) {
            return Integer.MAX_VALUE;
        } else {
            return (int) kb;
        }
    }
}
