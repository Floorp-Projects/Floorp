/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.util.SparseArray;

public final class ActivityResultHandlerMap {
    private final SparseArray<ActivityResultHandler> mMap = new SparseArray<ActivityResultHandler>();
    private int mCounter;

    public synchronized int put(ActivityResultHandler handler) {
        mMap.put(mCounter, handler);
        return mCounter++;
    }

    public synchronized ActivityResultHandler getAndRemove(int i) {
        ActivityResultHandler handler = mMap.get(i);
        mMap.delete(i);

        return handler;
    }
}
