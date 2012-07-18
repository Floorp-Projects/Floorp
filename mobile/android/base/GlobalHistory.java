/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.HashSet;
import java.util.LinkedList;
import java.util.Queue;
import java.util.Set;
import java.lang.ref.SoftReference;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.util.Log;

import org.mozilla.gecko.db.BrowserDB;

class GlobalHistory {
    private static final String LOGTAG = "GeckoGlobalHistory";

    private static GlobalHistory sInstance = new GlobalHistory();

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
        mHandler = GeckoAppShell.getHandler();
        mPendingUris = new LinkedList<String>();
        mVisitedCache = new SoftReference<Set<String>>(null);
        mNotifierRunnable = new Runnable() {
            public void run() {
                Set<String> visitedSet = mVisitedCache.get();
                if (visitedSet == null) {
                    // the cache was wiped away, repopulate it
                    Log.w(LOGTAG, "Rebuilding visited link set...");
                    visitedSet = new HashSet<String>();
                    Cursor c = BrowserDB.getAllVisitedHistory(GeckoApp.mAppContext.getContentResolver());
                    if (c.moveToFirst()) {
                        do {
                            visitedSet.add(c.getString(0));
                        } while (c.moveToNext());
                    }
                    mVisitedCache = new SoftReference<Set<String>>(visitedSet);
                    c.close();
                }

                // this runs on the same handler thread as the checkUriVisited code,
                // so no synchronization needed
                while (true) {
                    String uri = mPendingUris.poll();
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

    // Logic ported from nsNavHistory::CanAddURI.
    // http://mxr.mozilla.org/mozilla-central/source/toolkit/components/places/nsNavHistory.cpp#1272
    private boolean canAddURI(String uri) {
        if (uri == null || uri.length() == 0)
            return false;

        // First, heck the most common cases (HTTP, HTTPS) to avoid most of the work.
        if (uri.startsWith("http:") || uri.startsWith("https:"))
            return true;

        String scheme = Uri.parse(uri).getScheme();
        if (scheme == null)
            return false;

        // Now check for all bad things.
        if (scheme.equals("about") ||
            scheme.equals("imap") ||
            scheme.equals("news") ||
            scheme.equals("mailbox") ||
            scheme.equals("moz-anno") ||
            scheme.equals("view-source") ||
            scheme.equals("chrome") ||
            scheme.equals("resource") ||
            scheme.equals("data") ||
            scheme.equals("wyciwyg") ||
            scheme.equals("javascript"))
            return false;

        return true;
    }

    public void add(String uri) {
        if (!canAddURI(uri))
            return;

        BrowserDB.updateVisitedHistory(GeckoApp.mAppContext.getContentResolver(), uri);
        addToGeckoOnly(uri);
    }

    public void update(String uri, String title) {
        if (!canAddURI(uri))
            return;

        ContentResolver resolver = GeckoApp.mAppContext.getContentResolver();
        BrowserDB.updateHistoryTitle(resolver, uri, title);
    }

    public void checkUriVisited(final String uri) {
        mHandler.post(new Runnable() {
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
