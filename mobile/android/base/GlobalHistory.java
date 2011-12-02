/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kartikaya Gupta <kgupta@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.util.HashSet;
import java.util.LinkedList;
import java.util.Queue;
import java.util.Set;
import java.lang.ref.SoftReference;

import android.content.ContentValues;
import android.database.Cursor;
import android.os.Handler;
import android.provider.Browser;
import android.util.Log;

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
                    Cursor c = GeckoApp.mAppContext.getContentResolver().query(Browser.BOOKMARKS_URI,
                                    new String[] { Browser.BookmarkColumns.URL },
                                    Browser.BookmarkColumns.BOOKMARK + "=0 AND " +
                                    Browser.BookmarkColumns.VISITS + ">0",
                                    null,
                                    null);
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

    public void add(String uri) {
        Browser.updateVisitedHistory(GeckoApp.mAppContext.getContentResolver(), uri, true);
        Set<String> visitedSet = mVisitedCache.get();
        if (visitedSet != null) {
            visitedSet.add(uri);
        }
        GeckoAppShell.notifyUriVisited(uri);
    }

    public void update(String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(Browser.BookmarkColumns.TITLE, title);
        GeckoApp.mAppContext.getContentResolver().update(
                Browser.BOOKMARKS_URI,
                values,
                Browser.BookmarkColumns.URL + " = ?",
                new String[] { uri });
    }

    public void checkUriVisited(final String uri) {
        mHandler.post(new Runnable() {
            public void run() {
                // this runs on the same handler thread as the processing loop,
                // so synchronization needed
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
