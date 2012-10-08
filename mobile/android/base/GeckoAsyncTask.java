/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.app.Activity;
import android.os.Handler;

// AsyncTask runs onPostExecute on the thread it is constructed on
// We construct these off of the main thread, and we want that to run
// on the main UI thread, so this is a convenience class to do that
public abstract class GeckoAsyncTask<Params, Progress, Result> {
    public enum Priority { NORMAL, HIGH };

    private final Activity mActivity;
    private final Handler mBackgroundThreadHandler;
    private Priority mPriority = Priority.NORMAL;

    public GeckoAsyncTask(Activity activity, Handler backgroundThreadHandler) {
        mActivity = activity;
        mBackgroundThreadHandler = backgroundThreadHandler;
    }

    private final class BackgroundTaskRunnable implements Runnable {
        private Params[] mParams;

        public BackgroundTaskRunnable(Params... params) {
            mParams = params;
        }

        public void run() {
            final Result result = doInBackground(mParams);
            mActivity.runOnUiThread(new Runnable() {
                public void run() {
                    onPostExecute(result);
                }
            });
        }
    }

    public final void execute(final Params... params) {
        BackgroundTaskRunnable runnable = new BackgroundTaskRunnable(params);
        if (mPriority == Priority.HIGH)
            mBackgroundThreadHandler.postAtFrontOfQueue(runnable);
        else
            mBackgroundThreadHandler.post(runnable);
    }

    public final GeckoAsyncTask<Params, Progress, Result> setPriority(Priority priority) {
        mPriority = priority;
        return this;
    }

    protected abstract Result doInBackground(Params... params);
    protected abstract void onPostExecute(Result result);
}
