/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.os.SystemClock;

/**
 * A custom-built data structure to assist with measuring draw times.
 *
 * This class maintains a fixed-size circular buffer of DisplayPortMetrics
 * objects and associated timestamps. It provides only three operations, which
 * is all we require for our purposes of measuring draw times. Note
 * in particular that the class is designed so that even though it is
 * accessed from multiple threads, it does not require synchronization;
 * any concurrency errors that result from this are handled gracefully.
 *
 * Assuming an unrolled buffer so that mTail is greater than mHead, the data
 * stored in the buffer at entries [mHead, mTail) will never be modified, and
 * so are "safe" to read. If this reading is done on the same thread that
 * owns mHead, then reading the range [mHead, mTail) is guaranteed to be safe
 * since the range itself will not shrink.
 */
final class DrawTimingQueue {
    private static final String LOGTAG = "GeckoDrawTimingQueue";
    private static final int BUFFER_SIZE = 16;

    private final DisplayPortMetrics[] mMetrics;
    private final long[] mTimestamps;

    private int mHead;
    private int mTail;

    DrawTimingQueue() {
        mMetrics = new DisplayPortMetrics[BUFFER_SIZE];
        mTimestamps = new long[BUFFER_SIZE];
        mHead = BUFFER_SIZE - 1;
        mTail = 0;
    }

    /**
     * Add a new entry to the tail of the queue. If the buffer is full,
     * do nothing. This must only be called from the Java UI thread.
     */
    boolean add(DisplayPortMetrics metrics) {
        if (mHead == mTail) {
            return false;
        }
        mMetrics[mTail] = metrics;
        mTimestamps[mTail] = SystemClock.uptimeMillis();
        mTail = (mTail + 1) % BUFFER_SIZE;
        return true;
    }

    /**
     * Find the timestamp associated with the given metrics, AND remove
     * all metrics objects from the start of the queue up to and including
     * the one provided. Note that because of draw coalescing, the metrics
     * object passed in here may not be the one at the head of the queue,
     * and so we must iterate our way through the list to find it.
     * This must only be called from the compositor thread.
     */
    long findTimeFor(DisplayPortMetrics metrics) {
        // keep a copy of the tail pointer so that we ignore new items
        // added to the queue while we are searching. this is fine because
        // the one we are looking for will either have been added already
        // or will not be in the queue at all.
        int tail = mTail;
        // walk through the "safe" range from mHead to tail; these entries
        // will not be modified unless we change mHead.
        int i = (mHead + 1) % BUFFER_SIZE;
        while (i != tail) {
            if (mMetrics[i].fuzzyEquals(metrics)) {
                // found it, copy out the timestamp to a local var BEFORE
                // changing mHead or add could clobber the timestamp.
                long timestamp = mTimestamps[i];
                mHead = i;
                return timestamp;
            }
            i = (i + 1) % BUFFER_SIZE;
        }
        return -1;
    }

    /**
     * Reset the buffer to empty.
     * This must only be called from the compositor thread.
     */
    void reset() {
        // we can only modify mHead on this thread.
        mHead = (mTail + BUFFER_SIZE - 1) % BUFFER_SIZE;
    }
}
