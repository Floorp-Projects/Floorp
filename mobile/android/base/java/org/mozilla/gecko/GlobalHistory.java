/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.ref.SoftReference;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Queue;
import java.util.Set;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.util.GeckoBundle;
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
    SoftReference<Set<String>> mVisitedCache;   // cache of the visited URI list
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
            Set<String> visitedSet = mVisitedCache.get();
            if (visitedSet == null) {
                // The cache was wiped. Repopulate it.
                Log.w(LOGTAG, "Rebuilding visited link set...");
                final long start = SystemClock.uptimeMillis();
                final Cursor c = mDB.getAllVisitedHistory(mContentResolver);
                if (c == null) {
                    return;
                }

                try {
                    visitedSet = new HashSet<String>();
                    if (c.moveToFirst()) {
                        do {
                            visitedSet.add(c.getString(0));
                        } while (c.moveToNext());
                    }
                    mVisitedCache = new SoftReference<Set<String>>(visitedSet);
                    final long end = SystemClock.uptimeMillis();
                    final long took = end - start;
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

                if (visitedSet.contains(uri)) {
                    GeckoAppShell.notifyUriVisited(uri);
                }
            }

            mProcessing = false;
        }
    };

    private GlobalHistory() {
        mHandler = ThreadUtils.getBackgroundHandler();
        mPendingUris = new LinkedList<String>();
        mVisitedCache = new SoftReference<Set<String>>(null);
    }

    public void addToGeckoOnly(String uri) {
        Set<String> visitedSet = mVisitedCache.get();
        if (visitedSet != null) {
            visitedSet.add(uri);
        }
        GeckoAppShell.notifyUriVisited(uri);
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
}
