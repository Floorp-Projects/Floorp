/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.ref.SoftReference;
import java.security.NoSuchAlgorithmException;
import java.util.LinkedList;
import java.util.Queue;

import com.github.yonik.MurmurHash3;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;

class GlobalHistory {
    private static final String LOGTAG = "GeckoGlobalHistory";

    public static final String EVENT_URI_AVAILABLE_IN_HISTORY = "URI_INSERTED_TO_HISTORY";
    public static final String EVENT_PARAM_URI = "uri";

    private static final String TELEMETRY_HISTOGRAM_ADD = "FENNEC_GLOBALHISTORY_ADD_MS";
    private static final String TELEMETRY_HISTOGRAM_UPDATE = "FENNEC_GLOBALHISTORY_UPDATE_MS";
    private static final String TELEMETRY_HISTOGRAM_BUILD_VISITED_LINK = "FENNEC_GLOBALHISTORY_VISITED_BUILD_MS";

    private static final GlobalHistory sInstance = new GlobalHistory();

    static GlobalHistory getInstance() {
        return sInstance;
    }

    // this is the delay between receiving a URI check request and processing it.
    // this allows batching together multiple requests and processing them together,
    // which is more efficient.
    private static final long BATCHING_DELAY_MS = 100;

    private final Handler mHandler;                     // a background thread on which we can process requests

    //  Note: These fields are accessed through the NotificationRunnable inner class.
    final Queue<String> mPendingUris;           // URIs that need to be checked

    // This set can grow to tens of thousands of entries and several megabytes
    // of overhead when using HashSet<String>.  Compact this by storing
    // a 64-bit hash instead of the original String.
    SoftReference<SimpleLongOpenHashSet> mVisitedCache;  // cache of the visited URI list

    boolean mProcessing; // = false             // whether or not the runnable is queued/working

    private class NotifierRunnable implements Runnable {
        private final ContentResolver mContentResolver;
        private final BrowserDB mDB;

        public NotifierRunnable(final Context context) {
            mContentResolver = context.getContentResolver();
            mDB = BrowserDB.from(context);
        }

        @Override
        public void run() {
            MurmurHash3.LongPair pair = new MurmurHash3.LongPair();
            SimpleLongOpenHashSet visitedSet = mVisitedCache.get();
            if (visitedSet == null) {
                // The cache was wiped. Repopulate it.
                Log.w(LOGTAG, "Rebuilding visited link set...");
                final long start = SystemClock.uptimeMillis();
                final Cursor c = mDB.getAllVisitedHistory(mContentResolver);
                if (c == null) {
                    return;
                }

                try {
                    visitedSet = new SimpleLongOpenHashSet(c.getCount());
                    if (c.moveToFirst()) {
                        do {
                            visitedSet.add(hashUrl(c.getBlob(0), pair));
                        } while (c.moveToNext());
                    }
                    mVisitedCache = new SoftReference<SimpleLongOpenHashSet>(visitedSet);
                    final long end = SystemClock.uptimeMillis();
                    final long took = end - start;
                    Log.i(LOGTAG, "Rebuilt visited link set containing " + visitedSet.size() + " URLs in " + took + " ms");
                    Telemetry.addToHistogram(TELEMETRY_HISTOGRAM_BUILD_VISITED_LINK, (int) Math.min(took, Integer.MAX_VALUE));
                } finally {
                    c.close();
                }
            }

            // This runs on the same handler thread as the checkUriVisited code,
            // so no synchronization is needed.
            while (true) {
                final String uri = mPendingUris.poll();
                if (uri == null) {
                    break;
                }

                if (visitedSet.contains(hashUrl(uri.getBytes(StringUtils.UTF_8), pair))) {
                    GeckoAppShell.notifyUriVisited(uri);
                }
            }

            mProcessing = false;
        }
    };

    private GlobalHistory() {
        mHandler = ThreadUtils.getBackgroundHandler();
        mPendingUris = new LinkedList<String>();
        mVisitedCache = new SoftReference<SimpleLongOpenHashSet>(null);
    }

    public void addToGeckoOnly(String uri) {
        addUri(uri);
        GeckoAppShell.notifyUriVisited(uri);
    }

    // visible for testing
    void addUri(String uri) {
        MurmurHash3.LongPair pair = new MurmurHash3.LongPair();
        SimpleLongOpenHashSet visitedSet = mVisitedCache.get();
        if (visitedSet != null) {
            visitedSet.add(hashUrl(uri.getBytes(StringUtils.UTF_8), pair));
        }
    }

    // visible for testing
    boolean containsUri(String uri) {
        MurmurHash3.LongPair pair = new MurmurHash3.LongPair();
        SimpleLongOpenHashSet visitedSet = mVisitedCache.get();
        if (visitedSet == null) {
            return false;
        }
        return visitedSet.contains(hashUrl(uri.getBytes(StringUtils.UTF_8), pair));
    }

    public void add(final Context context, final BrowserDB db, String uri) {
        ThreadUtils.assertOnBackgroundThread();
        final long start = SystemClock.uptimeMillis();

        // stripAboutReaderUrl only removes about:reader if present, in all other cases the original string is returned
        final String uriToStore = ReaderModeUtils.stripAboutReaderUrl(uri);

        db.updateVisitedHistory(context.getContentResolver(), uriToStore);

        final long end = SystemClock.uptimeMillis();
        final long took = end - start;
        Telemetry.addToHistogram(TELEMETRY_HISTOGRAM_ADD, (int) Math.min(took, Integer.MAX_VALUE));
        addToGeckoOnly(uriToStore);
        dispatchUriAvailableMessage(uri);
    }

    @SuppressWarnings("static-method")
    public void update(final ContentResolver cr, final BrowserDB db, String uri, String title) {
        ThreadUtils.assertOnBackgroundThread();
        final long start = SystemClock.uptimeMillis();

        final String uriToStore = ReaderModeUtils.stripAboutReaderUrl(uri);

        db.updateHistoryTitle(cr, uriToStore, title);

        final long end = SystemClock.uptimeMillis();
        final long took = end - start;
        Telemetry.addToHistogram(TELEMETRY_HISTOGRAM_UPDATE, (int) Math.min(took, Integer.MAX_VALUE));
    }

    @WrapForJNI(stubName = "CheckURIVisited", calledFrom = "gecko")
    private static void checkUriVisited(final String uri) {
        getInstance().checkVisited(uri);
    }

    @WrapForJNI(stubName = "MarkURIVisited", calledFrom = "gecko")
    private static void markUriVisited(final String uri) {
        final Context context = GeckoAppShell.getApplicationContext();
        final BrowserDB db = BrowserDB.from(context);
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                getInstance().add(context, db, uri);
            }
        });
    }

    @WrapForJNI(stubName = "SetURITitle", calledFrom = "gecko")
    private static void setUriTitle(final String uri, final String title) {
        final Context context = GeckoAppShell.getApplicationContext();
        final BrowserDB db = BrowserDB.from(context);
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                getInstance().update(context.getContentResolver(), db, uri, title);
            }
        });
    }

    /* protected */ void checkVisited(final String uri) {
        final String storedURI = ReaderModeUtils.stripAboutReaderUrl(uri);

        final NotifierRunnable runnable =
                new NotifierRunnable(GeckoAppShell.getApplicationContext());
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                // this runs on the same handler thread as the processing loop,
                // so no synchronization needed
                mPendingUris.add(storedURI);
                if (mProcessing) {
                    // there's already a runnable queued up or working away, so
                    // no need to post another
                    return;
                }
                mProcessing = true;
                mHandler.postDelayed(runnable, BATCHING_DELAY_MS);
            }
        });
    }

    private void dispatchUriAvailableMessage(String uri) {
        final GeckoBundle message = new GeckoBundle();
        message.putString(EVENT_PARAM_URI, uri);
        EventDispatcher.getInstance().dispatch(EVENT_URI_AVAILABLE_IN_HISTORY, message);
    }

    /** Calculate hash of input. */
    private static long hashUrl(byte[] input, MurmurHash3.LongPair pair) {
        MurmurHash3.murmurhash3_x64_128(input, 0, input.length, 0, pair);
        return pair.val1 ^ pair.val2;
    }
}
