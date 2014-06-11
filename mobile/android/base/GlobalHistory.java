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

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.ThreadUtils;

import android.database.Cursor;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;

class GlobalHistory {
    private static final String LOGTAG = "GeckoGlobalHistory";

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
    private final Queue<String> mPendingUris;           // URIs that need to be checked
    private SoftReference<Set<String>> mVisitedCache;   // cache of the visited URI list
    private final Runnable mNotifierRunnable;           // off-thread runnable used to check URIs
    private boolean mProcessing; // = false             // whether or not the runnable is queued/working

    private GlobalHistory() {
        mHandler = ThreadUtils.getBackgroundHandler();
        mPendingUris = new LinkedList<String>();
        mVisitedCache = new SoftReference<Set<String>>(null);
        mNotifierRunnable = new Runnable() {
            @Override
            public void run() {
                Set<String> visitedSet = mVisitedCache.get();
                if (visitedSet == null) {
                    // The cache was wiped. Repopulate it.
                    Log.w(LOGTAG, "Rebuilding visited link set...");
                    final long start = SystemClock.uptimeMillis();
                    final Cursor c = BrowserDB.getAllVisitedHistory(GeckoAppShell.getContext().getContentResolver());
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
                        Telemetry.HistogramAdd(TELEMETRY_HISTOGRAM_BUILD_VISITED_LINK, (int) Math.min(took, Integer.MAX_VALUE));
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
    }

    public void addToGeckoOnly(String uri) {
        Set<String> visitedSet = mVisitedCache.get();
        if (visitedSet != null) {
            visitedSet.add(uri);
        }
        GeckoAppShell.notifyUriVisited(uri);
    }

    public void add(String uri) {
        final long start = SystemClock.uptimeMillis();
        BrowserDB.updateVisitedHistory(GeckoAppShell.getContext().getContentResolver(), uri);
        final long end = SystemClock.uptimeMillis();
        final long took = end - start;
        Telemetry.HistogramAdd(TELEMETRY_HISTOGRAM_ADD, (int) Math.min(took, Integer.MAX_VALUE));
        addToGeckoOnly(uri);
    }

    @SuppressWarnings("static-method")
    public void update(String uri, String title) {
        final long start = SystemClock.uptimeMillis();
        BrowserDB.updateHistoryTitle(GeckoAppShell.getContext().getContentResolver(), uri, title);
        final long end = SystemClock.uptimeMillis();
        final long took = end - start;
        Telemetry.HistogramAdd(TELEMETRY_HISTOGRAM_UPDATE, (int) Math.min(took, Integer.MAX_VALUE));
    }

    public void checkUriVisited(final String uri) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                // this runs on the same handler thread as the processing loop,
                // so no synchronization needed
                mPendingUris.add(uri);
                if (mProcessing) {
                    // there's already a runnable queued up or working away, so
                    // no need to post another
                    return;
                }
                mProcessing = true;
                mHandler.postDelayed(mNotifierRunnable, BATCHING_DELAY_MS);
            }
        });
    }
}
