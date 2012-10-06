/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Handler;

// GeckoAsyncTask runs onPostExecute on the thread it is constructed on.
// To ensure that onPostExecute() runs on UI thread, do either of these:
//   1. construct GeckoAsyncTask on the UI thread
//   2. post to the view's UI thread, in onPostExecute()
public abstract class GeckoAsyncTask<Params, Progress, Result> {
    public enum Priority { NORMAL, HIGH };

    private final Handler mHandler;
    private Priority mPriority = Priority.NORMAL;

    public GeckoAsyncTask() {
        mHandler = new Handler();
    }

    private final class BackgroundTaskRunnable implements Runnable {
        private Params[] mParams;

        public BackgroundTaskRunnable(Params... params) {
            mParams = params;
        }

        public void run() {
            final Result result = doInBackground(mParams);
            mHandler.post(new Runnable() {
                public void run() {
                    onPostExecute(result);
                }
            });
        }
    }

    public final void execute(final Params... params) {
        BackgroundTaskRunnable runnable = new BackgroundTaskRunnable(params);
        if (mPriority == Priority.HIGH)
            GeckoBackgroundThread.getHandler().postAtFrontOfQueue(runnable);
        else
            GeckoBackgroundThread.getHandler().post(runnable);
    }

    public final GeckoAsyncTask<Params, Progress, Result> setPriority(Priority priority) {
        mPriority = priority;
        return this;
    }

    protected abstract Result doInBackground(Params... params);
    protected abstract void onPostExecute(Result result);
}
