/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.storage;

import android.os.SystemClock;
import android.support.annotation.VisibleForTesting;
import android.util.LruCache;

/**
 * In-memory cache to remember URLs from which loading icons has failed recently.
 */
public class FailureCache {
    /**
     * Retry loading failed icons after 4 hours.
     */
    private static final long FAILURE_RETRY_MILLISECONDS = 1000 * 60 * 60 * 4;

    private static final int MAX_ENTRIES = 25;

    private static FailureCache instance;

    public static synchronized FailureCache get() {
        if (instance == null) {
            instance = new FailureCache();
        }

        return instance;
    }

    private final LruCache<String, Long> cache;

    private FailureCache() {
        cache = new LruCache<>(MAX_ENTRIES);
    }

    /**
     * Remember this icon URL after loading from it (over the network) has failed.
     */
    public void rememberFailure(String iconUrl) {
        cache.put(iconUrl, SystemClock.elapsedRealtime());
    }

    /**
     * Has loading from this URL failed previously and recently?
     */
    public boolean isKnownFailure(String iconUrl) {
        synchronized (cache) {
            final Long failedAt = cache.get(iconUrl);
            if (failedAt == null) {
                return false;
            }

            if (failedAt + FAILURE_RETRY_MILLISECONDS < SystemClock.elapsedRealtime()) {
                // The wait time has passed and we can retry loading from this URL.
                cache.remove(iconUrl);
                return false;
            }
        }

        return true;
    }

    @VisibleForTesting
    public void evictAll() {
        cache.evictAll();
    }
}
